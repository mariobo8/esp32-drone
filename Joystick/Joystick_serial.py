import pygame
import sys
import math
import serial
import time
import threading

'''
This code sends joystick commands to a Base Station esp32 through the usb port
then the ESP32 will send data using WiFi ESP NOW. It also receives and displays telemetry data.
'''
ser = serial.Serial("/dev/ttyACM0", 1000000, timeout=0.1)

# Initialize Pygame
pygame.init()

# Set up the display
WIDTH, HEIGHT = 1000, 700  # Increased size to accommodate telemetry
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Quadcopter Control with Telemetry")

# Colors
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
GRAY = (200, 200, 200)

# Fonts
font = pygame.font.Font(None, 36)
small_font = pygame.font.Font(None, 24)

# Arming Controls
armed = False

# Body Controls
throttle = 0
pitch = 0
roll = 0
yaw = 0

# Telemetry Data
telemetry_data = ""
telemetry_lock = threading.Lock()

def send_command():
    command = f"A:{int(armed)},T:{throttle},P:{pitch},R:{roll},Y:{yaw}"
    try:
        ser.write(command.encode('utf-8'))
        time.sleep(0.1)
        print(f"Sent: {command}")
    except serial.SerialException as e:
        print(f"Error sending data: {e}")

def read_telemetry():
    global telemetry_data
    while True:
        try:
            line = ser.readline().decode('utf-8').strip()
            if line.startswith("T:"):
                with telemetry_lock:
                    telemetry_data = line[2:]  # Remove "T:" prefix
        except Exception as e:
            print(f"Error reading telemetry: {e}")
        time.sleep(0.1)

def draw_arm_button():
    button_color = GREEN if armed else RED
    pygame.draw.rect(screen, button_color, (600, 50, 150, 50))
    text = font.render("ARMED" if armed else "DISARMED", True, BLACK)
    text_rect = text.get_rect(center=(675, 75))
    screen.blit(text, text_rect)

def draw_interface():
    screen.fill(WHITE)
    
    # Draw throttle bar
    pygame.draw.rect(screen, GRAY, (50, 50, 50, 400))
    throttle_height = throttle * 4  # Scale to fit 400 pixels
    pygame.draw.rect(screen, GREEN, (50, 450 - throttle_height, 50, throttle_height))
    screen.blit(font.render("Throttle", True, BLACK), (40, 460))
    
    # Draw pitch/roll indicator
    pygame.draw.circle(screen, GRAY, (300, 250), 100)
    indicator_x = 300 + roll
    indicator_y = 250 - pitch
    pygame.draw.circle(screen, RED, (indicator_x, indicator_y), 10)
    screen.blit(font.render("Pitch/Roll", True, BLACK), (260, 360))
    
    # Draw yaw indicator
    pygame.draw.circle(screen, GRAY, (600, 250), 100, 2)
    angle = math.radians(yaw * 3.6 - 90)  # Convert yaw to angle, -90 to start at top
    end_x = 600 + 100 * math.cos(angle)
    end_y = 250 + 100 * math.sin(angle)
    pygame.draw.line(screen, BLUE, (600, 250), (end_x, end_y), 4)
    screen.blit(font.render("Yaw", True, BLACK), (585, 360))
    
    # Draw values
    screen.blit(font.render(f"Throttle: {throttle}", True, BLACK), (50, 500))
    screen.blit(font.render(f"Pitch: {pitch}", True, BLACK), (50, 540))
    screen.blit(font.render(f"Roll: {roll}", True, BLACK), (300, 500))
    screen.blit(font.render(f"Yaw: {yaw}", True, BLACK), (300, 540))
    
    # Draw instructions
    instructions = [
        "Up/Down: Throttle Up/Down",
        "Left/Right: Yaw Left/Right",
        "W/S: Pitch Forward/Back",
        "A/D: Roll Left/Right",
        "Space: Arm/Disarm",
        "Q: Quit"
    ]
    for i, instruction in enumerate(instructions):
        text = small_font.render(instruction, True, BLACK)
        screen.blit(text, (750, 50 + i * 25))

    draw_arm_button()

    # Draw telemetry data
    with telemetry_lock:
        telemetry_lines = telemetry_data.split(',')
    pygame.draw.rect(screen, GRAY, (50, 580, 900, 100))
    screen.blit(font.render("Telemetry Data:", True, BLACK), (60, 590))
    for i, line in enumerate(telemetry_lines):
        text = small_font.render(line.strip(), True, BLACK)
        screen.blit(text, (60, 620 + i * 25))

    pygame.display.flip()

# Start telemetry reading thread
telemetry_thread = threading.Thread(target=read_telemetry, daemon=True)
telemetry_thread.start()

running = True
clock = pygame.time.Clock()
space_pressed = False

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
    
    keys = pygame.key.get_pressed()
    
    # Arm The Drone
    if keys[pygame.K_SPACE]:
        if not space_pressed:
            armed = not armed
            throttle = 0
            yaw = 0
            print("Drone armed" if armed else "Drone disarmed")
        space_pressed = True
    else:
        space_pressed = False

    # Throttle control
    if keys[pygame.K_UP]:
        throttle = min(throttle + 5, 98)
    if keys[pygame.K_DOWN]:
        throttle = max(throttle - 5, 0)
    
    # Yaw control
    if keys[pygame.K_RIGHT]:
        yaw = min(yaw + 2, 100)
    elif keys[pygame.K_LEFT]:
        yaw = max(yaw - 2, -100)
    else:
        yaw = int(yaw * 0)  # Gradually return to zero
    
    # Pitch control
    if keys[pygame.K_w]:
        pitch = min(pitch + 5, 100)
    elif keys[pygame.K_s]:
        pitch = max(pitch - 5, -100)
    else:
        pitch = int(pitch * 0)  # Gradually return to zero
    
    # Roll control
    if keys[pygame.K_d]:
        roll = min(roll + 5, 100)
    elif keys[pygame.K_a]:
        roll = max(roll - 5, -100)
    else:
        roll = int(roll * 0)  # Gradually return to zero
    
    # Quit
    if keys[pygame.K_q]:
        running = False
    
    send_command()
    draw_interface()
    clock.tick(30)  # 30 FPS

pygame.quit()
sys.exit()
