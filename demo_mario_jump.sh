#!/bin/bash

# Demo script: Make Mario jump and move right
# Requires Super Mario Bros loaded in FCEUX

BASE_URL="http://localhost:8080/api"

echo "=== Mario Jump Demo ==="
echo "Make sure Super Mario Bros is loaded and Mario is on screen!"
echo "Press Enter to continue..."
read

# Move right and jump
echo "Moving right and jumping..."
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["Right", "A"], "duration_ms": 200}'

sleep 0.3

# Continue moving right  
echo "Continue moving right..."
curl -s -X POST "$BASE_URL/input/port/1/press" \
  -H "Content-Type: application/json" \
  -d '{"buttons": ["Right"], "duration_ms": 500}'

echo
echo "Demo complete!"