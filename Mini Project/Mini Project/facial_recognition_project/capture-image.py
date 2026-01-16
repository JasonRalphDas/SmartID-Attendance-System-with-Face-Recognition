import os
import cv2

# Define the directory for saving faces in the current folder
faces_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "faces")

# Create 'faces' directory if not exists
if not os.path.exists(faces_dir):
    os.makedirs(faces_dir)
    print(f"Directory '{faces_dir}' created")

# Take input for the person's name
name = input("Enter name: ")

# Create video capture object
cap = cv2.VideoCapture(0)

# Check if camera is detected
if not cap.isOpened():
    print("Error: Camera not detected!")
    exit()

print("Press 'c' to capture an image, 'q' to quit")

while True:
    success, frame = cap.read()
    if not success:
        print("Failed to capture frame! Check camera connection.")
        break

    cv2.imshow("Frame", frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('c'):
        filename = os.path.join(faces_dir, name + ".jpg")
        cv2.imwrite(filename, frame)
        print("Image Saved -", filename)
    elif key == ord('q'):
        break

# Release camera and close windows
cap.release()
cv2.destroyAllWindows()
print("Capture completed.")