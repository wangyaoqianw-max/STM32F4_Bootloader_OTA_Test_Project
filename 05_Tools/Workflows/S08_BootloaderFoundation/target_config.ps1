function Set-S08ToolkitTarget {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [ValidateSet("application", "bootloader")]
        [string]$Target = "application"
    )

    if ($Target -eq "bootloader") {
        $Configuration.PROJECT_KEIL_PROJECT_FILE = "03_Firmware\Bootloader\OTA_Bootloader\MDK-ARM\OTA_Bootloader.uvprojx"
        $Configuration.PROJECT_KEIL_TARGET = "OTA_Bootloader"
        $Configuration.PROJECT_OUTPUT_DIR = "03_Firmware\Bootloader\OTA_Bootloader\MDK-ARM\Objects"
        $Configuration.PROJECT_APP_AXF = "03_Firmware\Bootloader\OTA_Bootloader\MDK-ARM\Objects\OTA_Bootloader.axf"
        $Configuration.PROJECT_APP_HEX = "03_Firmware\Bootloader\OTA_Bootloader\MDK-ARM\Objects\OTA_Bootloader.hex"
    }

    return $Configuration
}
