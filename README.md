# DEPRECATED

This project is deprecated in favor of etheos: https://github.com/EndlessOpenSource/etheos

# MSEOSERV
EOSERV source modified for use with Microsoft Visual Studio

This is a fork of the EOSERV project available at https://eoserv.net/

The original source used for this project was forked from rev 532 of the EOSERV source code.

### Additional Configuration - Debugging with Visual Studio

In order to debug using Visual Studio, you must set up the working directory. This is a user-specific setting that is not checked in to source control.

1. Select **Project**->**Properties** from the menu
2. Select **Debugging** under **Configuration Properties**
3. Change **Working Directory** to **$(ProjectDir)..\bin\**

Note that the working directory must be changed individually for each project configuration (i.e. DEBUG+MYSQL, RELEASE+MYSQL, etc.)
