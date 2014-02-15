@echo off

set XPJ="xpj4.exe"


%XPJ% -v 1 -t VC9 -p WIN32 -x blockchain.xpj
%XPJ% -v 1 -t VC9 -p WIN64 -x blockchain.xpj

%XPJ% -v 1 -t VC10 -p WIN32 -x blockchain.xpj
%XPJ% -v 1 -t VC10 -p WIN64 -x blockchain.xpj

cd ..
cd vc9win64

goto cleanExit

:pauseExit
pause

:cleanExit

