# Video Streaming API

Real-time MJPEG video streaming for computer vision and machine learning integration.

## Overview

The video streaming API provides access to the emulator's video output as an MJPEG
stream. This is designed for integration with OpenCV, TensorFlow, PyTorch, and other
CV/ML frameworks.

**Key Features:**
- Native 256x240 NES resolution (no scaling artifacts)
- Compatible with `cv2.VideoCapture()` out of the box
- Configurable frame rate and JPEG quality
- Optional grayscale mode for reduced bandwidth
- Zero overhead when no clients are connected

## Quick Start

### Python with OpenCV

```python
import cv2

# Connect to the MJPEG stream
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream")

while True:
    ret, frame = cap.read()
    if ret:
        # frame is a 240x256x3 BGR numpy array
        cv2.imshow('FCEUX', frame)

        # Your CV/ML processing here
        # gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        # prediction = model.predict(frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
```

### With Custom Parameters

```python
import cv2

# Lower FPS and quality for turn-based games
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?fps=15&quality=85")

# Or with grayscale for reduced bandwidth
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?grayscale=1")
```

### Check Stream Status

```python
import requests

response = requests.get("http://localhost:8080/api/video/info")
info = response.json()
print(f"Stream available: {info['available']}")
print(f"Active clients: {info['active_clients']}")
print(f"Total frames: {info['total_frames']}")
```

---

## Endpoints

### GET /api/video/stream

Returns an MJPEG video stream of the emulator display.

#### Query Parameters

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `fps` | int | 30 | 1-60 | Target frame rate |
| `quality` | int | 90 | 1-100 | JPEG compression quality |
| `grayscale` | 0\|1 | 0 | - | Output grayscale instead of color |

#### Response

- **Content-Type:** `multipart/x-mixed-replace; boundary=fceux-frame-boundary`
- **Format:** MJPEG stream (each frame is a separate JPEG image)

#### Frame Properties

| Property | Value |
|----------|-------|
| Width | 256 pixels |
| Height | 240 pixels |
| Color Space | RGB (delivered as BGR by OpenCV) |
| Pixel Format | 24-bit RGB (8 bits per channel) |

#### Error Responses

| Status | Description |
|--------|-------------|
| 503 | No game loaded |

#### Examples

```bash
# View stream with ffplay
ffplay "http://localhost:8080/api/video/stream"

# View stream with VLC
vlc "http://localhost:8080/api/video/stream"

# Save frames with ffmpeg
ffmpeg -i "http://localhost:8080/api/video/stream" -vframes 100 frame_%04d.jpg
```

---

### GET /api/video/info

Returns information about the video stream status.

#### Response

```json
{
    "available": true,
    "width": 256,
    "height": 240,
    "active_clients": 1,
    "total_frames": 12345
}
```

#### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `available` | boolean | Whether streaming is available (game loaded) |
| `width` | int | Frame width in pixels (always 256) |
| `height` | int | Frame height in pixels (always 240) |
| `active_clients` | int | Number of currently connected streaming clients |
| `total_frames` | uint64 | Total frames captured since emulator started |

---

## Performance Guidelines

### Frame Rate Selection

| Use Case | Recommended FPS | Notes |
|----------|-----------------|-------|
| Turn-based games | 10-15 | NPCs move slowly, no fast action |
| General gameplay | 30 | Good balance for most games |
| Action/platformers | 60 | Maximum frame rate for precise timing |
| RL training (fast) | 60 | When training speed matters |
| RL training (stable) | 30 | When consistency matters more than speed |

### Quality Selection

| Quality | Approx. Size | Use Case |
|---------|--------------|----------|
| 100 | 50-100 KB | Archival, lossless-ish |
| 90 | 20-40 KB | High quality CV (recommended) |
| 75 | 10-20 KB | Balanced quality/bandwidth |
| 50 | 5-10 KB | Low bandwidth situations |

### Latency Budget

Typical latency breakdown (at default settings):

| Stage | Time |
|-------|------|
| Frame capture | ~0.5 ms |
| JPEG encoding | ~3-5 ms |
| Network transfer | ~1-2 ms (localhost) |
| **Total** | **~5-10 ms** |

---

## Architecture

### Data Flow

```
NES PPU (ppu.cpp)
    │
    ▼
XBuf (256x240 palette-indexed)
    │
    ▼
VideoStreamManager::onFrameComplete()
    │
    ├── Check: Any clients connected?
    │   └── No → Return immediately (zero overhead)
    │
    └── Yes → VideoFrameBuffer::captureFrame()
              │
              ├── Convert palette indices to RGB
              ├── Write to double-buffer
              ├── Swap buffers (atomic)
              └── Signal waiting threads
                  │
                  ▼
              Streaming Thread(s)
                  │
                  ├── Wait for frame
                  ├── Rate limit to target FPS
                  ├── Encode to JPEG
                  └── Write to HTTP response
```

### Thread Safety

The implementation uses a double-buffering pattern:

1. **Emulator thread** writes to Buffer A
2. **Streaming threads** read from Buffer B
3. After write completes, buffers are swapped atomically
4. No mutex contention during normal operation

This ensures the emulator is never blocked waiting for network I/O.

---

## Troubleshooting

### Stream doesn't start

**Check:** Is a game loaded?
```bash
curl http://localhost:8080/api/video/info
# Should show "available": true
```

**Check:** Is the REST API enabled?
```bash
curl http://localhost:8080/api/system/ping
# Should return {"status": "ok"}
```

### OpenCV VideoCapture fails to open

```python
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream")
if not cap.isOpened():
    print("Failed to open stream")
    # Check if FCEUX is running and REST API is enabled
```

**Common causes:**
- FCEUX not running
- REST API not enabled (build with `-DREST_API=ON`)
- Wrong port (default is 8080)
- No game loaded

### Frames are corrupted or glitchy

**Try:** Reduce frame rate
```python
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?fps=15")
```

**Try:** Increase JPEG quality
```python
cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?quality=95")
```

### High CPU usage

The video streaming is designed to be efficient, but if you're seeing high CPU:

1. **Reduce FPS** - Use `?fps=15` for turn-based games
2. **Use grayscale** - Use `?grayscale=1` to reduce encoding work
3. **Reduce quality** - Use `?quality=75` for faster encoding

---

## Examples

### Save Screenshots from Stream

```python
import cv2
import time

cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?fps=1")

for i in range(10):
    ret, frame = cap.read()
    if ret:
        cv2.imwrite(f"screenshot_{i:04d}.png", frame)
        print(f"Saved screenshot_{i:04d}.png")
    time.sleep(1)

cap.release()
```

### Real-time Object Detection

```python
import cv2
import numpy as np

# Load your trained model
# model = load_your_model()

cap = cv2.VideoCapture("http://localhost:8080/api/video/stream?fps=30")

while True:
    ret, frame = cap.read()
    if not ret:
        continue

    # Preprocess for your model
    # input_tensor = preprocess(frame)

    # Run inference
    # detections = model.predict(input_tensor)

    # Draw results
    # frame = draw_detections(frame, detections)

    cv2.imshow('Detection', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
```

### Reinforcement Learning Agent

```python
import cv2
import numpy as np
import requests

class FCEUXEnv:
    def __init__(self, fps=30):
        self.cap = cv2.VideoCapture(
            f"http://localhost:8080/api/video/stream?fps={fps}"
        )
        self.api_base = "http://localhost:8080/api"

    def get_frame(self):
        """Get current game frame as numpy array."""
        ret, frame = self.cap.read()
        if ret:
            # Convert BGR to RGB
            return cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        return None

    def press_button(self, button):
        """Press a button on controller 1."""
        requests.post(
            f"{self.api_base}/input/port/0/press",
            json={"buttons": [button]}
        )

    def release_button(self, button):
        """Release a button on controller 1."""
        requests.post(
            f"{self.api_base}/input/port/0/release",
            json={"buttons": [button]}
        )

    def close(self):
        self.cap.release()

# Usage
env = FCEUXEnv(fps=30)
frame = env.get_frame()
env.press_button("A")
env.close()
```

---

## See Also

- [Input API](input.md) - Controller input for game interaction
- [Memory API](memory.md) - RAM access for game state
- [Screenshot API](media.md) - Single frame capture
