import os

# Define incorrect and correct handler names
incorrect_name = "DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQ"
correct_name = "DMA1_Ch4_7_DMAMUX_OVR_IRQ"

# File extensions to search (adjust as needed)
file_extensions = (".c", ".h", ".cpp", ".hpp")

def replace_in_file(file_path):
    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as file:
            content = file.read()

        if incorrect_name in content:
            new_content = content.replace(incorrect_name, correct_name)
            with open(file_path, "w", encoding="utf-8", errors="ignore") as file:
                file.write(new_content)
            print(f"Replaced in: {file_path}")

    except Exception as e:
        print(f"Skipped {file_path} due to error: {e}")

def scan_and_replace():
    for root, _, files in os.walk("."):
        for name in files:
            if name.endswith(file_extensions):
                file_path = os.path.join(root, name)
                replace_in_file(file_path)

if __name__ == "__main__":
    scan_and_replace()
