import serial
import time
import csv
from datetime import datetime

# === CONFIG ===

SERIAL_PORT = 'COM5'  # Change to your port
BAUD_RATE = 9600
CSV_FILE = 'plates_log.csv'
RATE_PER_HOUR = 200


def find_latest_unpaid(plate):
    with open(CSV_FILE, 'r', newline='') as f:
        reader = csv.DictReader(f)
        entries = [row for row in reader if
                   row['Plate Number'].strip() == plate.strip() and row['Payment Status'] == '0']
        if not entries:
            return None
        latest = max(entries, key=lambda x: datetime.strptime(x['Timestamp'], "%Y-%m-%d %H:%M:%S"))
        return latest


def mark_as_paid(target_row):
    rows = []
    with open(CSV_FILE, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    for row in rows:
        if (row['Plate Number'] == target_row['Plate Number'] and
                row['Timestamp'] == target_row['Timestamp'] and
                row['Payment Status'] == '0'):
            row['Payment Status'] = '1'
            break

    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['Plate Number', 'Payment Status', 'Timestamp'])
        writer.writeheader()
        writer.writerows(rows)


def clean_plate(plate_raw):
    return plate_raw.replace('\x00', '').strip().upper()


def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
    print(f"Listening on {SERIAL_PORT}...")

    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()

            if not line or line == ",-1":
                continue

            print(f"Received: {line}")
            parts = [part.strip() for part in line.split(',')]
            if len(parts) != 2:
                print("Invalid format.")
                continue

            plate_raw, balance_str = parts
            plate = clean_plate(plate_raw)

            try:
                balance = int(balance_str)
            except ValueError:
                print("Invalid balance received.")
                continue

            entry = find_latest_unpaid(plate)
            if not entry:
                print(f"No unpaid entry for {plate}")
                ser.write(b'0\n')
                continue

            entry_time = datetime.strptime(entry['Timestamp'], "%Y-%m-%d %H:%M:%S")
            now = datetime.now()
            duration = (now - entry_time).total_seconds() / 3600
            due = int(RATE_PER_HOUR * max(1, round(duration)))

            print(f"Due for {plate}: {due} RWF")

            # Insufficient balance handling with top-up
            while balance < due:
                print(f"Insufficient balance: {balance} < {due}")
                ser.write(b'-1\n')  # Signal insufficient balance to Arduino

                # Wait for Arduino's "insufficient" confirmation
                start_time = time.time()
                confirmation = None
                while True:
                    if ser.in_waiting:
                        response = ser.readline().decode().strip()
                        if response == "insufficient":
                            confirmation = "insufficient"
                            break
                    if time.time() - start_time > 5:
                        print("No response from Arduino.")
                        break

                if confirmation != "insufficient":
                    break  # Exit loop if no confirmation

                # Prompt user for top-up
                choice = input("Would you like to top-up? (yes/no): ").strip().lower()
                if choice != 'yes':
                    print("Payment aborted.")
                    break

                # Get valid top-up amount
                while True:
                    try:
                        topup = int(input("Enter top-up amount (positive integer): "))
                        if topup > 0:
                            break
                        print("Amount must be positive.")
                    except ValueError:
                        print("Invalid input. Enter a number.")

                # Send top-up command to Arduino
                ser.write(f"topup,{topup}\n".encode())
                print(f"Sent top-up: {topup}")

                # Wait for new balance confirmation
                start_time = time.time()
                topped = False
                while True:
                    if ser.in_waiting:
                        response = ser.readline().decode().strip()
                        if response.startswith("topped,"):
                            try:
                                new_balance = int(response.split(',')[1])
                                balance = new_balance
                                print(f"New balance: {balance} RWF")
                                topped = True
                                break
                            except:
                                print("Error processing top-up.")
                                break
                    if time.time() - start_time > 5:
                        print("Top-up timeout.")
                        break

                if not topped:
                    continue  # Retry top-up

                # Recalculate due with updated time
                now = datetime.now()
                duration = (now - entry_time).total_seconds() / 3600
                due = int(RATE_PER_HOUR * max(1, round(duration)))
                print(f"Updated due: {due} RWF")

            if balance >= due:
                # Proceed with payment
                ser.write(f"{due}\n".encode())
                start_time = time.time()
                while True:
                    if ser.in_waiting:
                        response = ser.readline().decode().strip()
                        if response == "done":
                            print("Payment successful!")
                            mark_as_paid(entry)
                            break
                        elif response == "insufficient":
                            print("Unexpected insufficient balance after top-up.")
                            break
                    if time.time() - start_time > 5:
                        print("Confirmation timeout.")
                        break

        except KeyboardInterrupt:
            print("Exiting...")
            break
        except Exception as e:
            print(f"Error: {e}")

    ser.close()


if __name__ == "__main__":
    main()