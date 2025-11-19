import serial
import time
import struct


class ContinuousNavconMonitor:
    def __init__(self, port, baudrate=19200):
        """Initialize the continuous monitor with serial connection"""
        self.ser = serial.Serial(port, baudrate, timeout=1)
        self.cycle_count = 0
        self.sync_found = False
        self.packet_buffer = []
        self.current_cycle = 0  # Track current cycle in test
        self.selected_test = None

        # Color codes as per SCS
        self.COLORS = {
            'W': 0b000,  # White
            'R': 0b001,  # Red
            'G': 0b010,  # Green
            'B': 0b011,  # Blue
            'K': 0b100  # Black
        }

        # Test 1 Configuration -HANDLING GREEN WITH <5
        self.TEST_1 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'GWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WGW', 'incidence': 2, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WWW', 'incidence': 2, 'speed': [25, 25], 'rotation': 0, 'distance': 230}
        ]

        # Test 2 Configuration -HANDLING GREEN WITH <45
        self.TEST_2 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'GWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WGW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WGW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'GWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 3 Configuration -HANDLING GREEN WITH >45
        self.TEST_3 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'GWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WGW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WGW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'GWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 230},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 4 Configuration -HANDLING BLUE WITH <45 AND THEN DETECTING GREEN
        self.TEST_4 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'BWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WBW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WBW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'BWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 115, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'GWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WGW', 'incidence': 2, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WWW', 'incidence': 2, 'speed': [25, 25], 'rotation': 0, 'distance': 230}
        ]

        # Test 5 Configuration -HANDLING BLUE WITH <45 AND THEN DETECTING BLACK
        self.TEST_5 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'BWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WBW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WBW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'BWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 115, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'KWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WKW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WKW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'KWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 110, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 100, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 6 Configuration -HANDLING BLACK WITH <45
        self.TEST_6 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'KWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WKW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 223},
            {'colors': 'WKW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'KWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 30, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 20, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 100, 'distance': 0},
            {'colors': 'WWW', 'incidence': 30, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 7 Configuration -HANDLING BLACK WITH >45
        self.TEST_7 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'KWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WKW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WKW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'KWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 8 Configuration -HANDLING BLUE WITH >45
        self.TEST_8 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'BWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WBW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WBW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'BWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

        # Test 9 Configuration -HANDLING RED WITH >45
        self.TEST_9 = [
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 200},
            {'colors': 'RWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 220},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WRW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 265},
            {'colors': 'WRW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 2},
            {'colors': 'RWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 5},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 120},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 150},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 2, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 3, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [0, 0], 'rotation': 0, 'distance': 0},
            {'colors': 'WWW', 'incidence': 0, 'speed': [25, 25], 'rotation': 0, 'distance': 3}
        ]

    def select_test(self):
        """Let user select which test to run"""
        # Create a mapping of test numbers to test objects and their cycle counts
        test_mapping = {
            1: (self.TEST_1, len(self.TEST_1)),
            2: (self.TEST_2, len(self.TEST_2)),
            3: (self.TEST_3, len(self.TEST_3)),
            4: (self.TEST_4, len(self.TEST_4)),
            5: (self.TEST_5, len(self.TEST_5)),
            6: (self.TEST_6, len(self.TEST_6)),
            7: (self.TEST_7, len(self.TEST_7)),
            8: (self.TEST_8, len(self.TEST_8)),
            9: (self.TEST_9, len(self.TEST_9))
        }

        print("\nAvailable Tests:")
        for test_num, (test_obj, cycle_count) in test_mapping.items():
            if cycle_count > 0:  # Only show tests that have been configured
                print(f"{test_num}. Test {test_num} ({cycle_count} cycles)")
            else:
                print(f"{test_num}. Test {test_num} (NOT CONFIGURED)")

        while True:
            try:
                choice = input("Select test (1-9): ").strip()
                test_num = int(choice)

                if 1 <= test_num <= 9:
                    test_obj, cycle_count = test_mapping[test_num]
                    if cycle_count > 0:
                        self.selected_test = test_obj
                        print(f"Selected: Test {test_num} ({cycle_count} cycles)")
                        break
                    else:
                        print(f"Test {test_num} is not configured yet. Please add test data first.")
                else:
                    print("Invalid choice. Please enter a number between 1 and 9.")
            except ValueError:
                print("Invalid input. Please enter a number between 1 and 9.")
            except KeyboardInterrupt:
                print("\nExiting...")
                exit(0)

    def create_control_byte(self, sys_state, subsystem, internal_state):
        """Create control byte according to SCS format"""
        return (sys_state << 6) | (subsystem << 4) | internal_state

    def create_sensor_packet(self, s1_color, s2_color, s3_color):
        """Create sensor color packet according to SCS format"""
        # Sensor data packed as: DATA<8:6> = sensor1, DATA<5:3> = sensor2, DATA<2:0> = sensor3
        color_data = (self.COLORS[s1_color] << 6) | (self.COLORS[s2_color] << 3) | self.COLORS[s3_color]
        dat1 = (color_data >> 8) & 0xFF
        dat0 = color_data & 0xFF
        return dat1, dat0

    def send_packet(self, control_byte, dat1=0, dat0=0, dec=0):
        """Send a 4-byte packet to the MCU"""
        packet = bytes([control_byte, dat1, dat0, dec])
        self.ser.write(packet)
        print(f"  SENT: Control={control_byte:3d} | DAT1=0x{dat1:02X} | DAT0=0x{dat0:02X} | DEC=0x{dec:02X}")
        time.sleep(0.05)  # Small delay between packets

    def send_5_packets(self):
        """Send the standard 5 data packets based on current test cycle"""
        if self.current_cycle >= len(self.selected_test):
            print(f"\n--- TEST COMPLETED! All {len(self.selected_test)} cycles finished ---")
            return False

        cycle_config = self.selected_test[self.current_cycle]
        print(f"\n--- SENDING 5 DATA PACKETS (Cycle #{self.current_cycle + 1}/{len(self.selected_test)}) ---")
        print(f"Config: {cycle_config}")

        # 1. MDPS ROTATION packet
        print("1. MDPS ROTATION packet:")
        mdps_rotation_control = self.create_control_byte(2, 2, 2)  # SYS=MAZE, SUB=MDPS, IST=ROTATION
        rotation_angle = cycle_config['rotation']
        dat1_rot = (rotation_angle >> 8) & 0xFF
        dat0_rot = rotation_angle & 0xFF
        dec_rot = 2  # Left rotation (CCW)
        self.send_packet(mdps_rotation_control, dat1_rot, dat0_rot, dec_rot)

        # 2. MDPS SPEED packet
        print("2. MDPS SPEED packet:")
        mdps_speed_control = self.create_control_byte(2, 2, 3)  # SYS=MAZE, SUB=MDPS, IST=SPEED
        right_speed, left_speed = cycle_config['speed']
        self.send_packet(mdps_speed_control, right_speed, left_speed, 0)

        # 3. MDPS DISTANCE packet
        print("3. MDPS DISTANCE packet:")
        mdps_distance_control = self.create_control_byte(2, 2, 4)  # SYS=MAZE, SUB=MDPS, IST=DISTANCE
        distance = cycle_config['distance']
        dat1_dist = (distance >> 8) & 0xFF
        dat0_dist = distance & 0xFF
        self.send_packet(mdps_distance_control, dat1_dist, dat0_dist, 0)

        # 4. SENSOR COLORS packet
        print("4. SENSOR COLORS packet:")
        sensor_colors_control = self.create_control_byte(2, 3, 1)  # SYS=MAZE, SUB=SENSOR, IST=COLORS
        colors = cycle_config['colors']
        dat1_colors, dat0_colors = self.create_sensor_packet(colors[0], colors[1], colors[2])
        self.send_packet(sensor_colors_control, dat1_colors, dat0_colors, 0)

        # 5. SENSOR INCIDENCE packet
        print("5. SENSOR INCIDENCE packet:")
        sensor_incidence_control = self.create_control_byte(2, 3, 2)  # SYS=MAZE, SUB=SENSOR, IST=INCIDENCE
        incidence = cycle_config['incidence']
        self.send_packet(sensor_incidence_control, incidence, 0, 0)

        self.current_cycle += 1
        return True

    def wait_for_3_responses(self, timeout=10):
        """Wait for exactly 3 response packets from MCU"""
        print(f"\n--- WAITING FOR 3 RESPONSE PACKETS ---")
        responses = []
        start_time = time.time()

        while len(responses) < 3 and (time.time() - start_time) < timeout:
            if self.ser.in_waiting >= 4:
                try:
                    packet = self.ser.read(4)
                    if len(packet) == 4:
                        control, dat1, dat0, dec = struct.unpack('BBBB', packet)
                        responses.append((control, dat1, dat0, dec))
                        print(
                            f"  Response #{len(responses)}: Control={control:3d} | DAT1=0x{dat1:02X} | DAT0=0x{dat0:02X} | DEC=0x{dec:02X}")
                except:
                    continue
            time.sleep(0.01)

        if len(responses) < 3:
            print(f"⚠ WARNING: Only received {len(responses)}/3 response packets within timeout")
        else:
            print(f"✓ Successfully received all 3 response packets")

        return responses

    def check_for_sync_packet(self):
        """Check incoming bytes for the sync packet (147,0C,0C,00)"""
        while self.ser.in_waiting > 0:
            byte_data = self.ser.read(1)
            if byte_data:
                byte_value = byte_data[0]
                self.packet_buffer.append(byte_value)

                # Keep buffer to last 4 bytes for packet detection
                if len(self.packet_buffer) > 4:
                    self.packet_buffer.pop(0)

                # Check if we have a complete 4-byte packet
                if len(self.packet_buffer) == 4:
                    control, dat1, dat0, dec = self.packet_buffer

                    # Check for sync packet
                    if control == 147 and dat1 == 0x0C and dat0 == 0x0C and dec == 0x00:
                        print(f"\n{'=' * 60}")
                        print("✓ SYNC PACKET DETECTED: (147, 0C, 0C, 00)")
                        print("Starting test sequence...")
                        print(f"{'=' * 60}")
                        return True
                    else:
                        # Show all incoming bytes for monitoring
                        print(
                            f"Monitoring | Control={control:3d} | DAT1=0x{dat1:02X} | DAT0=0x{dat0:02X} | DEC=0x{dec:02X}")

        return False

    def run_test_sequence(self):
        """Run the selected test sequence"""
        while self.current_cycle < len(self.selected_test):
            try:
                self.cycle_count += 1

                # Send 5 packets for current cycle
                if not self.send_5_packets():
                    break  # Test completed

                # Wait for 3 responses
                responses = self.wait_for_3_responses()

                # Analyze final response if we got all 3
                if len(responses) == 3:
                    final_response = responses[2]  # Third packet is navigation decision
                    control, dat1, dat0, dec = final_response

                    print(f"\n--- NAVIGATION ANALYSIS (Cycle #{self.current_cycle}/{len(self.selected_test)}) ---")
                    if dec == 0:
                        if dat1 == 0:
                            print(f"→ STOP: Right={dat1}mm/s, Left={dat0}mm/s")
                        else:
                            print(f"→ FORWARD: Right={dat1}mm/s, Left={dat0}mm/s")
                    elif dec == 1:
                        print(f"→ REVERSE: Right={dat1}mm/s, Left={dat0}mm/s")
                    elif dec == 2:
                        angle = (dat1 << 8) | dat0
                        print(f"→ ROTATE LEFT: {angle}°")
                    elif dec == 3:
                        angle = (dat1 << 8) | dat0
                        print(f"→ ROTATE RIGHT: {angle}°")
                    else:
                        print(f"→ UNKNOWN COMMAND: DEC={dec:02X}")

                print(f"\n{'─' * 60}")
                if self.current_cycle < len(self.selected_test):
                    print(f"Cycle #{self.current_cycle} completed. Moving to next cycle...")
                    print(f"{'─' * 60}")
                    time.sleep(0.5)  # Brief pause between cycles
                else:
                    print(f"TEST COMPLETED! All {len(self.selected_test)} cycles finished successfully.")
                    print(f"{'─' * 60}")
                    break

            except KeyboardInterrupt:
                print(f"\n\nStopping after {self.cycle_count} cycles...")
                break
            except Exception as e:
                print(f"Error in cycle {self.cycle_count}: {e}")
                time.sleep(1)  # Wait before retrying

    def monitor_and_exchange(self):
        """Main monitoring function that waits for sync then starts test sequence"""
        print(f"Connected to serial port at 19200 baud")
        print("Monitoring for sync packet (147, FF, FF, 00)...")
        print("Press Ctrl+C to stop")
        print("-" * 60)

        try:
            # Phase 1: Monitor for sync packet
            while not self.sync_found:
                if self.check_for_sync_packet():
                    self.sync_found = True
                    break
                time.sleep(0.01)

            # Phase 2: Run test sequence
            if self.sync_found:
                time.sleep(0.5)  # Brief pause after sync
                self.run_test_sequence()

        except KeyboardInterrupt:
            print("\nStopping monitor...")
        except Exception as e:
            print(f"Error: {e}")
        finally:
            self.close()

    def close(self):
        """Close serial connection"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("Serial connection closed.")


def list_serial_ports():
    """List available serial ports"""
    import serial.tools.list_ports

    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for i, port in enumerate(ports):
        print(f"{i + 1}. {port.device} - {port.description}")
    return ports


if __name__ == "__main__":
    # List available ports
    ports = list_serial_ports()

    if not ports:
        print("No serial ports found!")
        exit(1)

    # Let user choose port or specify manually
    print("\nOptions:")
    print("1. Enter port number from list above")
    print("2. Enter custom port path (e.g., COM3, /dev/ttyUSB0)")

    choice = input("Enter choice (1 or 2): ").strip()

    if choice == "1":
        try:
            port_num = int(input("Enter port number: ")) - 1
            if 0 <= port_num < len(ports):
                selected_port = ports[port_num].device
            else:
                print("Invalid port number!")
                exit(1)
        except ValueError:
            print("Invalid input!")
            exit(1)
    elif choice == "2":
        selected_port = input("Enter port path: ").strip()
    else:
        print("Invalid choice!")
        exit(1)

    # Ask for baud rate (default 19200)
    baud_input = input("Enter baud rate (default 19200): ").strip()
    baudrate = int(baud_input) if baud_input else 19200

    print(f"\nStarting NAVCON test bench on {selected_port} at {baudrate} baud...")

    try:
        monitor = ContinuousNavconMonitor(selected_port, baudrate)
        monitor.select_test()  # Let user select test before starting
        monitor.monitor_and_exchange()
    except serial.SerialException as e:
        print(f"Serial connection error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")