# MSEOSERV
EOSERV source modified for use with Microsoft Visual Studio

This project is based on the source code from the EOSERV project, available at https://eoserv.net/.

The original source used for this project was cloned from rev 532 of the EOSERV source code.

### Additional Configuration - Debugging with Visual Studio

In order to debug using Visual Studio, you must set up the working directory. This is a user-specific setting that is not checked in to source control.

1. Select **Project**->**Properties** from the menu
2. Select **Debugging** under **Configuration Properties**
3. Change **Working Directory** to **$(ProjectDir)..\bin\**

Note that the working directory must be changed individually for each project configuration (i.e. DEBUG+MYSQL, RELEASE+MYSQL, etc.)
