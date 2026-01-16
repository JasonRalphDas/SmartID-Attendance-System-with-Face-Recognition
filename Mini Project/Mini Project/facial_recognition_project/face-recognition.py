
import cv2
import os
import numpy as np
import serial
import time

# ================== SERIAL SETUP ==================
PORT = 'COM5'       # Change to your NodeMCU COM port
BAUD = 115200       # Match NodeMCU Serial.begin()
ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)       # Let NodeMCU boot
ser.reset_input_buffer()

print("🔌 Connecting to NodeMCU...")
boot_ready = False
boot_start = time.time()

while time.time() - boot_start < 10:
    if ser.in_waiting:
        line = ser.readline().decode(errors='ignore').strip()
        if line:
            print("NodeMCU Boot:", line)
            if "Waiting for START" in line or "NodeMCU READY" in line:
                boot_ready = True
                break

if not boot_ready:
    print("⚠️ No new boot logs, assuming NodeMCU is already running and ready.")

# ================== FACE TRAINING ==================
def train_face_recognizer():
    faces_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "faces")
    
    if not os.path.exists(faces_dir):
        print(f"Error: Faces directory '{faces_dir}' not found.")
        return None, {}
    
    image_paths = [os.path.join(faces_dir, f) for f in os.listdir(faces_dir) 
                  if f.lower().endswith(('.png', '.jpg', '.jpeg'))]
    
    if len(image_paths) == 0:
        print("No face images found in the faces directory.")
        return None, {}
    
    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
    recognizer = cv2.face.LBPHFaceRecognizer_create()
    
    faces, labels, label_names = [], [], {}
    current_label = 0
    
    print("📸 Loading face images for training...")
    
    for image_path in image_paths:
        person_name = os.path.splitext(os.path.basename(image_path))[0]
        if person_name not in label_names:
            label_names[person_name] = current_label
            current_label += 1
        
        image = cv2.imread(image_path)
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        
        detected_faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
        if len(detected_faces) == 0:
            print(f"⚠️ No face detected in {image_path}")
            continue
        
        for (x, y, w, h) in detected_faces:
            face_roi = cv2.resize(gray[y:y+h, x:x+w], (100, 100))
            faces.append(face_roi)
            labels.append(label_names[person_name])
    
    if len(faces) == 0:
        print("❌ No faces detected in training images.")
        return None, {}
    
    print(f"✅ Training with {len(faces)} face images...")
    recognizer.train(faces, np.array(labels))
    
    label_to_name = {v: k for k, v in label_names.items()}
    print("✅ Training complete.")
    return recognizer, label_to_name

# ================== MAIN FUNCTION ==================
def main():
    recognizer, label_to_name = train_face_recognizer()
    if recognizer is None:
        return
    
    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
    
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("❌ Error: Could not open camera.")
        return
    
    print("🎥 Continuous face recognition started. Press Ctrl+C to exit.")

    try:
        while True:
            # ---------------- FACE RECOGNITION LOOP ----------------
            recognized = False
            while not recognized:
                ret, frame = cap.read()
                if not ret:
                    print("❌ Failed to grab frame")
                    return
                
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
                
                if len(faces) > 0:
                    (x, y, w, h) = faces[0]
                    face_roi = cv2.resize(gray[y:y+h, x:x+w], (100, 100))
                    
                    label, confidence = recognizer.predict(face_roi)
                    
                    if label in label_to_name and (100 - confidence) >= 10:
                        name = label_to_name[label].upper()
                        print(f"✅ {name} recognized! Sending START to NodeMCU...")
                        ser.write(b"START\n")
                        recognized = True
                        break

                # Also check if NodeMCU has logs while waiting
                while ser.in_waiting:
                    line = ser.readline().decode(errors='ignore').strip()
                    if line:
                        print("[NodeMCU]", line)
            
            # ---------------- WAIT FOR RFID SCAN ----------------
            waiting_rfid = True
            while waiting_rfid:
                if ser.in_waiting:
                    line = ser.readline().decode(errors='ignore').strip()
                    if line:
                        print("[NodeMCU]", line)
                        # Exit RFID wait when card is scanned
                        if "CARD_SCANNED" in line:
                            print("🔄 RFID scan complete. Ready for next face...\n")
                            waiting_rfid = False
                time.sleep(0.1)

    except KeyboardInterrupt:
        print("\n🛑 Exiting continuous mode...")

    cap.release()
    cv2.destroyAllWindows()
    ser.close()

if __name__ == "__main__":
    main()
