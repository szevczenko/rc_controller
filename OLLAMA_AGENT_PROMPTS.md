# Ollama Agent Prompts for RC Controller

This file contains ready-to-paste prompts for VS Code + Ollama, and a practical ESP-IDF setup/build flow.

## 1. Base Prompt (paste once at start of a new chat)

You are a senior embedded firmware engineer specializing in ESP-IDF, C11, and FreeRTOS.
Project: custom RC controller with TX on ESP32-S3 and RX on ESP32-WROOM.
Your goal is to deliver safe, incremental implementation with verification after each step.

Architecture rules:
1. Keep radio transport abstracted. Application logic must not call ESP-NOW directly.
2. RC protocol must be transport-agnostic (encode/decode raw bytes).
3. Failsafe is mandatory: no packet for 500 ms means safe state.
4. Arming is mandatory before motor output.
5. Store configuration in NVS. No hardcoded calibration values.
6. Channel normalized range is -1000 to +1000.
7. Prioritize correctness and safety over optimization.

Working style:
1. Start each task with a short 3-6 step plan.
2. Implement only the minimal scope for the requested milestone.
3. Run build/tests after changes and report outcomes.
4. If information is missing, choose the simplest reasonable default and state it.
5. Avoid large refactors unless required.

Response format:
1. What changed
2. Why
3. How verified
4. Next single step

Use these project files as source of truth:
- project-plan.md
- IMPLEMENTATION_PLAN.md

## 2. Prompt for MR-01 (Project Skeleton)

Implement MR-01 (Project skeleton) from IMPLEMENTATION_PLAN.md.

Scope:
1. Create transmitter and receiver ESP-IDF project skeletons.
2. Create shared components structure.
3. Add sdkconfig.defaults for TX and RX targets.
4. Add OTA-ready partitions configuration.
5. Keep all changes minimal and buildable.

Verification:
1. Build transmitter with target esp32s3.
2. Build receiver with target esp32.
3. Report exact build results.

Do not implement runtime features beyond MR-01.

## 3. Prompt for MR-02 (CI Build Matrix)

Implement MR-02 from IMPLEMENTATION_PLAN.md.

Scope:
1. Add CI pipeline/workflow for build checks.
2. Include matrix for receiver and transmitter targets.
3. Keep CI minimal: compile only, no hardware flashing.

Verification:
1. Validate workflow syntax.
2. Explain how to trigger CI.

## 4. Prompt for MR-03 (ADC Abstraction)

Implement MR-03 (ADC abstraction) from IMPLEMENTATION_PLAN.md.

Scope:
1. Add input_adc component with clear public header and source.
2. Provide init and read APIs.
3. Keep hardware assumptions explicit in config structs.
4. Keep it ready for two gimbals in later milestones.

Verification:
1. Build TX target.
2. Show example usage from application side.

## 5. Prompt for Code Review Mode

Review the current changes for bugs, regressions, and safety risks.

Output requirements:
1. Findings first, ordered by severity.
2. For each finding, provide exact file and location.
3. Include why it is a risk and a concrete fix proposal.
4. If no findings, state that explicitly and list remaining test gaps.

## 6. When to Send Each Prompt

1. Start new chat:
- Send Base Prompt once.
- Then send exactly one MR prompt (for example MR-01).

2. During implementation:
- Do not combine multiple MRs in one message.
- After MR is complete, send next MR prompt in a new message.

3. After context switch or long pause:
- Re-send a short reminder line plus the active MR prompt.

## 7. ESP-IDF Environment Setup (based on your machine)

From workspace root (rc-controller), run:

1. Activate ESP-IDF environment:
source ../esp-idf-v5.5.4/export.sh

Expected key output:
- Activating ESP-IDF 5.5
- Setting IDF_PATH to /home/dmyshe@ad.global/projects/esp-idf-v5.5.4
- Checking python dependencies ... OK
- Establishing a new ESP-IDF environment ... OK
- Done! You can now compile ESP-IDF projects.

2. Optional cleanup of old tool versions (disk space):
python /home/dmyshe@ad.global/projects/esp-idf-v5.5.4/tools/idf_tools.py uninstall

3. Optional cleanup including downloaded archives:
python /home/dmyshe@ad.global/projects/esp-idf-v5.5.4/tools/idf_tools.py uninstall --remove-archives

## 8. Build Commands

Run from workspace root after environment activation.

If transmitter and receiver folders already exist:

1. Build transmitter:
cd transmitter
idf.py set-target esp32s3
idf.py build

2. Build receiver:
cd ../receiver
idf.py set-target esp32
idf.py build

3. Return to root:
cd ..

If folders do not exist yet:
- Run MR-01 first to create the project skeleton.

## 9. Quick One-Liner You Can Paste to Ollama

Implement MR-01 from IMPLEMENTATION_PLAN.md now. Keep scope minimal, create TX/RX ESP-IDF skeleton, shared components, sdkconfig defaults, and OTA-ready partition config, then run builds for esp32s3 and esp32 and report exact results.
