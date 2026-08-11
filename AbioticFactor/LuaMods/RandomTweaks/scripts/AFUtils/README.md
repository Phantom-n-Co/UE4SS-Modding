# Abiotic Factor Utility functions library
The library is based on my [BaseUtils](https://github.com/igromanru/BaseUtils-UE4SS).    
It's a single table named `AFUtils` which contains all the functions and variables.  
Different functionalities are split into modules, which get automatically imported into the `AFUtils.lua`.

## How to use
The library is supposed to be used as [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules) and cloned into a folder named `AFUtils`.  
1. Recursively clone or add as submodule the Repository to the `Scripts` directory of your mod into a folder with name `AFUtils`
2. Use `local AFUtils = require("AFUtils.AFUtils")` to get access to the library
3. (optional) You can add `require("AFUtils.AFUtilsDebug")` to get access to the "Log.." function, which are useful for debugging

## Examples
You can find all mods where I use AFUtils here:  
https://github.com/search?q=+user%3Aigromanru++igromanru%2FAFUtils-UE4SS+path%3A**%2F.gitmodules&type=code&query=UE4SS-AF++user%3Aigromanru++igromanru%2FAFUtils-UE4SS+path%3A**%2F.gitmodules