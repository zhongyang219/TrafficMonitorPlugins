**[简体中文](plugin_download.md) | English**

# TrafficMonitor Plugin Download

It is welcome to develop your own plugin for TrafficMonitor. If you want to submit your plugin, please send email to zhongyang219@hotmail.com and attach the download URL. Please marked "TrafficMonitor Plugin" in the email.

## How to develop the plugin

About how to develop the plugin for TrafficMonitor, please refer to [Plugin Development Guide](https://github.com/zhongyang219/TrafficMonitor/wiki/Plugin-Development-Guide).

## Plugin Usage Instructions

Please download the corresponding version (32 bit or 64 bit)  According to the version of TrafficMonitor you use. After downloaded, please put the dll file in the "plugins" directory where TrafficMonitor.exe is located (Create it if directory "plugins" is not exist). Then restart TrafficMonitor, the plugin will be loaded automatically. 

For the detail description of the plugin usage instruction, please refer to the following link:

[插件功能 · zhongyang219/TrafficMonitor Wiki (github.com)](https://github.com/zhongyang219/TrafficMonitor/wiki/插件功能)

## Plugin download

### Time date plugin

![image-20211216220605288](images/image-20211216220605288.png)

<img src="images/image-20220614195748677.png" alt="image-20220614195748677" style="zoom:80%;" />

A plugin used to display the date and time.

* Version: V1.00

* Download url: 

  GitHub: [Release DateTime_V1.00 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/DateTime_V1.00)

  Gitee: [DateTime_V1.00 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/DateTime_V1.00)

---

### Weather plugin

![image-20211225234420165](images/image-20211225234420165.png)

<img src="images/image-20211225234519524.png" alt="image-20211225234519524" style="zoom:80%;" />

A plugin for displaying the weather. Support manual selection of cities (only cities in China is supported), support display of today's and tomorrow's weather.

* Version: V1.03

* Download url: 

  GitHub: [Release Weather_V1.03 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Weather_V1.03)

  Gitee: [Weather_V1.03 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Weather_V1.03)

---

### WeatherPro

![img-weather-pro](images/img-weather-pro.png)

A plugin for displaying the weather. Support web fetching and weather API two data sources.

* Author: [Haojia521](https://github.com/Haojia521)

* Home page: [Haojia521/TrafficMonitorPlugins (github.com)](https://github.com/Haojia521/TrafficMonitorPlugins)

* Download url: [WeatherPro-releases](https://github.com/Haojia521/TrafficMonitorPlugins/releases)

---

### Battery plugin

![battery](images/battery.png)

<img src="images/image-20211226220834772.png" alt="image-20211226220834772" style="zoom:80%;" />

A plugin for displaying the battery level of your computer.

* Version: V1.03

* Download url:

  GitHub: [Release Battery_V1.03 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Battery_V1.03)

  Gitee: [Battery_V1.03 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Battery_V1.03)

---

### Stock plugin

![image-20250308114302152](images/image-20250308114302152.png)

Displays real-time trading information for specified stocks. Use the Sina interface.

- **sh indicates Shanghai Stock Exchange, sz indicates Shenzhen Stock Exchange, and so on.**

* Author: [CListery](https://github.com/CListery)

* Download url:

  GitHub: [Release Stock_V1.13 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Stock_V1.13)

  Gitee: [Stock_V1.13 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/Stock_V1.13)

---

### Battery power detection plugin

![](images/155976271-b3e58b7a-d3ec-442d-8107-c0c69a2d7610.png)

A plugin for displaying the battery power consumption. At the same time, the remaining battery and battery voltage information can be displayed in the mouse tool tip.

* Author: [AzulEterno](https://github.com/AzulEterno)

* Home page: [AzulEterno/Plugins-For-TrafficMonitor (github.com)](https://github.com/AzulEterno/Plugins-For-TrafficMonitor)

* Download url: [Releases · AzulEterno/PowerMonPlugin-For-TrafficMonitor](https://github.com/AzulEterno/PowerMonPlugin-For-TrafficMonitor/releases)

---

### Tomato clock plugin

![](images/img-pomodoro-timer.png)

A plugin Providing basic tomato time management functions. The red icon represents the working period and the green represents the rest period.

* Author: [Haojia521](https://github.com/Haojia521)

* Download url: [PomodoroTimer-releases](https://github.com/Haojia521/TrafficMonitorPlugins/releases)

---

### Text reader plugin

![image-20220701210850528](images/image-20220701210850528.png)

The text reader plugin can be used to read text files in the taskbar. Including chapter recognition, bookmarking, automatic page turning and other functions.

Use the mouse click or down arrow key to turn the page backward, the up arrow keys to turn the page forward. The left and right arrow keys to move the page one character at a time.

* Version: V1.02

* Download url:

  GitHub: [Release TextReader_V1.02 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/TextReader_V1.02)

  Gitee: [TextReader_V1.02 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/TextReader_V1.02)

---

### Hardware Monitor plugin

![](images/420555677-53bd3ac9-1c55-4212-aa68-1fe5711e9fbc.png)

Description:

- Hardware monitor plugin used the third-party library LibreHardwareMonitor. It can be used in both the Standard and Lite versions of TrafficMonitor. But since the Lite version does not have administrator privileges, it may not be possible to display all the hardware information.
- Unzip the file, place `HardwareMonitor.dll` in the `plugins` directory, and place `LibreHardwareMonitorLib.dll` to TrafficMonitor directory (NOT `plugins` directory). Since the standard version of TrafficMonitor contains a `LibreHardwareMonitorLib.dll`, so this file can be ignored.
- Start TrafficMonitor, open "Plugin Manager" dialog, select "Hardware Monitor plugin", click "Options", click "Add monitoring Items" button, right click the item to be monitored, select "Add to monitoring items", and close the dialog.
- Close all dialogs, restart TrafficMonitor, right-click the taskbar window, select "Display Settings", the added hardware monitoring items can be seen in the dialog.

* Version: V1.00

* Download url:

  GitHub: [Release HardwareMonitor_V1.00 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/HardwareMonitor_V1.00)

  Gitee: [HardwareMonitor_V1.00 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/HardwareMonitor_V1.00)

---

### IP address plugin

![image-20250308114927871](images/image-20250308114927871.png)

A plugin to display the local IP address.

* Version: V1.00

* Download url:

  GitHub: [Release IpAddress_V1.00 · zhongyang219/TrafficMonitorPlugins](https://github.com/zhongyang219/TrafficMonitorPlugins/releases/tag/IpAddress_V1.00)

  Gitee: [IpAddress_V1.00 · zhongyang219/TrafficMonitorPlugins - Gitee.com](https://gitee.com/zhongyang219/TrafficMonitorPlugins/releases/tag/IpAddress_V1.00)

---

### Lua Plugin

A TrafficMonitor plugin to support writing plugin by lua script.

* Author: [compilelife](https://github.com/compilelife)

* Homepage: [compilelife/TrafficMonitorLuaPlugin: Missing Plugin for TrafficMonitor to support lua](https://github.com/compilelife/TrafficMonitorLuaPlugin)

* Download url: [Releases · compilelife/TrafficMonitorLuaPlugin](https://github.com/compilelife/TrafficMonitorLuaPlugin/releases)

