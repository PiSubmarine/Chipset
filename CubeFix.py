import os

# 1. Config for Global IRQ Name Fix
incorrect_irq = "DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQ"
correct_irq = "DMA1_Ch4_7_DMAMUX_OVR_IRQ"

# 2. Config for CMake Fix
cmake_path = "cmake/stm32cubemx/CMakeLists.txt"
cmake_old = "target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})"
cmake_new = "target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_LIBS})"

# 3. Config for Interrupt File Fix (Adding I2C3 externs)
it_c_path = "Core/Src/stm32u0xx_it.c"
it_anchor = "extern DMA_HandleTypeDef hdma_i2c2_tx;"
it_addition = "\nextern DMA_HandleTypeDef hdma_i2c3_rx;\nextern DMA_HandleTypeDef hdma_i2c3_tx;"

file_extensions = (".c", ".h", ".cpp", ".hpp", ".txt")

def patch_specific_files():
    # --- Patch 1: CMakeLists.txt ---
    if os.path.exists(cmake_path):
        with open(cmake_path, "r", encoding="utf-8") as f:
            content = f.read()
        if cmake_old in content:
            with open(cmake_path, "w", encoding="utf-8") as f:
                f.write(content.replace(cmake_old, cmake_new))
            print(f"[SUCCESS] Patched: {cmake_path} (Added PRIVATE)")
        elif "PRIVATE" in content:
            print(f"[INFO] Skipping {cmake_path}: 'PRIVATE' already exists.")
    else:
        print(f"[ERROR] File not found: {cmake_path}")

    # --- Patch 2: stm32u0xx_it.c ---
    if os.path.exists(it_c_path):
        with open(it_c_path, "r", encoding="utf-8") as f:
            content = f.read()
        if it_anchor in content:
            if "extern DMA_HandleTypeDef hdma_i2c3_tx;" not in content:
                new_content = content.replace(it_anchor, it_anchor + it_addition)
                with open(it_c_path, "w", encoding="utf-8") as f:
                    f.write(new_content)
                print(f"[SUCCESS] Patched: {it_c_path} (Added I2C3 DMA externs)")
            else:
                print(f"[INFO] Skipping {it_c_path}: I2C3 externs already exist.")
        else:
            print(f"[WARNING] Anchor not found in {it_c_path}: '{it_anchor}'")
    else:
        print(f"[ERROR] File not found: {it_c_path}")

def scan_and_replace_irq():
    """Original logic for global IRQ rename"""
    found_any = False
    for root, _, files in os.walk("."):
        for name in files:
            if name.endswith(file_extensions):
                file_path = os.path.join(root, name)
                try:
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                        content = f.read()
                    if incorrect_irq in content:
                        with open(file_path, "w", encoding="utf-8", errors="ignore") as f:
                            f.write(content.replace(incorrect_irq, correct_irq))
                        print(f"[SUCCESS] Replaced IRQ in: {file_path}")
                        found_any = True
                except Exception as e:
                    print(f"[ERROR] Could not process {file_path}: {e}")

    if not found_any:
        print(f"[INFO] No instances of '{incorrect_irq}' found in the project.")

if __name__ == "__main__":
    print("Starting CubeMX Patching Script...")
    print("-" * 40)
    patch_specific_files()
    scan_and_replace_irq()
    print("-" * 40)
    print("Patching process finished.")