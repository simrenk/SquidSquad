import cv2
import mediapipe as mp
from time import time
import time

from mediapipe.tasks import python
from mediapipe.tasks.python import vision
from mediapipe.tasks.python.vision.hand_landmarker import HandLandmark
from pathlib import Path
from pythonosc.udp_client import SimpleUDPClient


model_path = str(Path(__file__).parent / "hand_landmarker.task")
wekinator = SimpleUDPClient("127.0.0.1", 6448)

BaseOptions = mp.tasks.BaseOptions
vision = mp.tasks.vision
HandLandmarker = vision.HandLandmarker
HandLandmarkerOptions = vision.HandLandmarkerOptions
HandLandmarkerResult = vision.HandLandmarkerResult
VisionRunningMode = vision.RunningMode
drawing_utils = vision.drawing_utils
drawing_styles = vision.drawing_styles
HAND_CONNECTIONS = vision.HandLandmarksConnections.HAND_CONNECTIONS
latest_result = None



def on_result(result, output_image, timestamp_ms):
  # print(f"{timestamp_ms}: {len(result.hand_landmarks)} hand(s)")
  global latest_result
  latest_result = result

  both_hands = {"Left": None, "Right": None}
  for hand, handedness in zip(result.hand_landmarks, result.handedness):
    handedness = handedness[0].category_name
    if both_hands[handedness] is not None: #if the hand we registered has already been populated, fill the data in the other hand's slot
      handedness = "Left" if handedness == "Right" else "Right"
    both_hands[handedness] = normalize(hand)
  padding = [0.0] * 63
  inputs = (both_hands["Left"] or padding) + (both_hands["Right"] or padding)
  wekinator.send_message("/wek/inputs", inputs)

def normalize(hand_landmark):
  wrist = hand_landmark[HandLandmark.WRIST]
  knuckle = hand_landmark[HandLandmark.MIDDLE_FINGER_MCP]
  palm = ((knuckle.x - wrist.x) ** 2 + (knuckle.y - wrist.y) ** 2) ** 0.5
  palm = palm or 1e-6 #in case zero
  normalized_hands = []

  for position_name in HandLandmark:
    point = hand_landmark[position_name]
    normalized_hands += [(point.x - wrist.x) / palm, #Div by palm so everything is expressed in palm lengths (and not affected by distance) 
                          (point.y - wrist.y) / palm, 
                          (point.z - wrist.z) / palm] 
  return normalized_hands #should be 63
   
options = HandLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=model_path),
    running_mode=VisionRunningMode.LIVE_STREAM,
    num_hands=2,
    result_callback=on_result)

cam = cv2.VideoCapture(0)

with HandLandmarker.create_from_options(options) as landmarker:  
   while cam.isOpened():
    success, image = cam.read()
    if not success:
      # print("Ignoring empty camera frame.")
      break
    ts = time.monotonic_ns() // 1_000_000
    rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)    
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    landmarker.detect_async(mp_image, ts)

    if latest_result and latest_result.hand_landmarks:
      for hand in latest_result.hand_landmarks:
          drawing_utils.draw_landmarks(
              image,
              hand,
              HAND_CONNECTIONS,
              drawing_styles.get_default_hand_landmarks_style(),
              drawing_styles.get_default_hand_connections_style(),
          )

    cv2.imshow('MediaPipe Hands', cv2.flip(image, 1))
    if cv2.waitKey(5) & 0xFF == 27:
      break
