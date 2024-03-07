import cv2
import numpy as np
import re
import sys


def display_image(image_bytes, x, y, height=240, width=320):
	# Convert the received bytes to a NumPy array.
	image_array = np.array(image_bytes, dtype=np.uint8)
	image_array = np.reshape(image_array, (height, width) )
	color_image = cv2.cvtColor(image_array, cv2.COLOR_GRAY2BGR)
	# draw a circle at the given point
	cv2.circle(color_image, (x, y), radius=15, color=(0, 0, 255), thickness=2)
	# Display the image using OpenCV.
	cv2.imshow('Received Image', color_image)
	cv2.waitKey(0)
	cv2.destroyAllWindows()


def read_from_file(file_path, height=240, width=320):
	with open(file_path, 'r') as file:
		# Read lines from the file
		lines = file.readlines()
		if len(lines) < (2* height*width/16) + 6:
			raise Exception("file not long enough")
		
	raw_log_text = "".join(lines)
	# get data for each picture between "Picture Start" and "Picture End" labels
	pics_pattern = re.compile(r"image_data: Picture Start([\s\S]*?)image_data: Picture End")
	pics = re.findall(pics_pattern, "".join(raw_log_text))
	print(f"total pictures found: {len(pics)}")
	for pic in pics:
		# Extract raw image bytes and display
		byte_pattern = re.compile(r'image_data: ((?:[0-9a-f]{2} )+)\n')
		raw_bytes = [int(byte, 16) for match in byte_pattern.finditer(pic) for byte in match.group(1).split()]
		point_pattern = re.compile(r"image_data: Highest point is \(x=(\d+), y=(\d+)\)")
		x, y = re.search(point_pattern, pic).groups()
		display_image(raw_bytes, int(x), int(y))


def main():
	if len(sys.argv) != 2:
		print("Usage: scan.py [log_file.txt]")
		return
	_, input_file_path = sys.argv
	read_from_file(input_file_path)


if __name__ == "__main__":
	main()
