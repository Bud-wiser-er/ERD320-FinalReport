# Reverse Distance Update - Angle-Based Implementation

## Summary

Updated `navcon_core.cpp` to use **angle-dependent reverse distances** instead of a single fixed value.

---

## Changes Made

### Change 1: Removed Fixed Constant (Line 6)

**Before:**
```cpp
const uint16_t REVERSE_DISTANCE = 65;  // mm to reverse before rotation
```

**After:**
```cpp
// Reverse distances based on angle (removed single constant, now calculated based on angle)
```

### Change 2: Added Angle-Based Logic (Lines 500-528)

**Before:**
```cpp
case NAVCON_REVERSE: {
    if (current_distance >= REVERSE_DISTANCE) {
        // Stop and transition to rotate
    }
    return createReversePacket();
}
```

**After:**
```cpp
case NAVCON_REVERSE: {
    // Calculate reverse distance based on angle
    uint16_t reverse_distance;
    if (navcon_status.line_detection.initial_angle > 45) {
        reverse_distance = 75;  // 75mm for steep angles (>45°)
    } else {
        reverse_distance = 55;  // 55mm for normal angles (≤45°)
    }

    if (current_distance >= reverse_distance) {
        Serial.printf("Reverse complete (distance: %d mm, target: %d mm for angle %d°)\n",
                    current_distance, reverse_distance,
                    navcon_status.line_detection.initial_angle);
        // Stop and transition to rotate
    }

    Serial.printf("Reversing... distance: %d mm (target: %d mm for angle %d°)\n",
                current_distance, reverse_distance,
                navcon_status.line_detection.initial_angle);
    return createReversePacket();
}
```

---

## New Behavior

| Detected Angle | Reverse Distance | Use Case |
|----------------|------------------|----------|
| **≤ 45°** | **55 mm** | Normal line crossing (S2 sensor detects first) |
| **> 45°** | **75 mm** | Steep angle (Edge sensor S1/S3 detects first) |

---

## Why Different Distances?

### Normal Angles (≤45°):
- S2 (center sensor) crosses the line first
- We know the exact crossing point
- Need less reverse to get back to center
- **55mm is sufficient**

### Steep Angles (>45°):
- S1 or S3 (edge sensor) triggers first
- S2 hasn't crossed yet (we're past the line)
- Need more reverse to get robot centered over the line
- **75mm accounts for sensor spacing + positioning**

---

## Debug Output

The updated code now prints debug information showing:
- Current reverse distance traveled
- Target reverse distance
- The angle that determined the target

**Example output:**
```
Reversing... distance: 20 mm (target: 55 mm for angle 22°)
Reversing... distance: 40 mm (target: 55 mm for angle 22°)
Reverse complete (distance: 55 mm, target: 55 mm for angle 22°) - stopping before rotate
```

```
Reversing... distance: 25 mm (target: 75 mm for angle 68°)
Reversing... distance: 50 mm (target: 75 mm for angle 68°)
Reverse complete (distance: 75 mm, target: 75 mm for angle 68°) - stopping before rotate
```

---

## Testing Recommendations

### Upload and Test:
1. Upload updated firmware to your Arduino
2. Run NAVCON tester with all angle scenarios
3. Monitor Serial output to verify:
   - Normal angles use 55mm
   - Steep angles use 75mm
   - Robot centers correctly on lines

### Observe Behavior:
- **22° GREEN line:** Should reverse 55mm
- **35° GREEN line:** Should reverse 55mm
- **Steep >45° GREEN:** Should reverse 75mm
- **BLUE/BLACK walls:** Should reverse based on angle provided

### Fine-Tuning:
If robot is not centered after reversing:

**For normal angles (adjust line 506):**
- Too far back: Decrease to 50mm
- Not far enough: Increase to 60mm

**For steep angles (adjust line 504):**
- Too far back: Decrease to 70mm
- Not far enough: Increase to 80mm

---

## Files Modified

- ✅ `navcon_core.cpp` - Updated reverse distance calculation
- ℹ️ `navcon_core.h` - No changes needed (structure already supports angle storage)

---

**Update complete!** The robot will now use appropriate reverse distances based on the detected line angle.
