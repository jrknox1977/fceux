#!/bin/bash

# Test script for FCEUX Input timing functionality
# Tests that button presses are released after the specified duration

BASE_URL="http://localhost:8080/api"

echo "=== FCEUX Input Timing Test ==="
echo
echo "This test will:"
echo "1. Press the A button for 500ms (~30 frames)"
echo "2. Check status every 100ms to see when it's released"
echo

# Press A for 500ms
echo "Pressing A button for 500ms..."
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A"], "duration_ms": 500}' | jq .

# Check status multiple times
for i in {1..8}; do
  sleep 0.1
  echo -n "After ${i}00ms: "
  STATUS=$(curl -s "$BASE_URL/input/status")
  A_PRESSED=$(echo "$STATUS" | jq -r '.port1.buttons.A')
  echo "A button = $A_PRESSED"
done

echo
echo "Test complete! The A button should have been released around 500ms."