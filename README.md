# DomeControl
Observatory dome control

### Настройка Clion:
1. Прописать в настройках cmake путь до toolchain-файла:
   -DCMAKE_TOOLCHAIN_FILE=<PATH_TO_TOOLCHAIN_FILE>
   
2. EditConfigurations (параметры запуска):
   * Executable: `run.bat`
   * Program arguments: `$CMakeCurrentTargetName$`
   * Working directory: `$CMakeCurrentBuildDir$`
   
Максимальные размеры платы: 120 * 80
