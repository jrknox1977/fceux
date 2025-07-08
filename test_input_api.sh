#!/bin/bash

# Test script for FCEUX Input Control REST API
# Assumes FCEUX is running with a game loaded

BASE_URL="http://localhost:8080/api"

echo "=== FCEUX Input API Test ==="
echo

echo "1. Testing GET /api/input/status"
curl -s "$BASE_URL/input/status" | jq .
echo

echo "2. Testing POST /api/input/port/1/press - Press A button for 100ms"
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["A"], "duration_ms": 100}' | jq .
echo

sleep 0.2

echo "3. Testing POST /api/input/port/1/press - Press Right+B for 50ms"
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["Right", "B"], "duration_ms": 50}' | jq .
echo

sleep 0.1

echo "4. Testing POST /api/input/port/1/state - Set specific state"
curl -s -X POST "$BASE_URL/input/port/1/state" \
  -H "Content-Type: application/json" \
  -d '{"A": true, "B": false, "Select": false, "Start": false, "Up": false, "Down": false, "Left": false, "Right": true}' | jq .
echo

sleep 0.1

echo "5. Testing POST /api/input/port/1/release - Release all buttons"
curl -s -X POST "$BASE_URL/input/port/1/release" \
  -H "Content-Type: application/json" | jq .
echo

echo "6. Testing error handling - invalid button name"
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["X"], "duration_ms": 50}' | jq .
echo

echo "Test complete!"