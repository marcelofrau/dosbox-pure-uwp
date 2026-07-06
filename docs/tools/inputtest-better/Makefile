WATCOM = C:\Apps\OW
WCC = $(WATCOM)\binnt\wcc.exe
WLINK = $(WATCOM)\binnt\wlink.exe
CFLAGS = -ms -I=$(WATCOM)\h
LDFLAGS = option quiet file
OUTDIR = dist
HELLO_EXE = $(OUTDIR)\hello.exe
INPUT_EXE = $(OUTDIR)\inputtest.exe
ZIP = $(OUTDIR)\inputtest-better.dosz
DEST = E:\PC\DOSBoxPure\inputtest-better.dosz

all: $(ZIP)

define pscompile
powershell -NoProfile -Command "$$env:WATCOM='$(WATCOM)'; $$env:PATH='$(WATCOM)\binnt;'+$$env:PATH; & '$(WCC)' $(CFLAGS) $(1).c; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE }; Move-Item $(1).obj '$(OUTDIR)\$(1).obj' -Force"
endef

define pslink
powershell -NoProfile -Command "$$env:WATCOM='$(WATCOM)'; $$env:PATH='$(WATCOM)\binnt;'+$$env:PATH; & '$(WLINK)' $(LDFLAGS) '$(OUTDIR)\$(1).obj' name '$(OUTDIR)\$(1).exe' library clibs.lib library graph.lib libpath '$(WATCOM)\lib286\dos' format dos; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE }"
endef

$(OUTDIR):
	if not exist $(OUTDIR) mkdir $(OUTDIR)

$(HELLO_EXE): hello.c | $(OUTDIR)
	$(call pscompile,hello)
	$(call pslink,hello)

$(INPUT_EXE): inputtest.c | $(OUTDIR)
	$(call pscompile,inputtest)
	$(call pslink,inputtest)

$(ZIP): $(HELLO_EXE) $(INPUT_EXE)
	powershell -NoProfile -Command "Compress-Archive -Path '$(HELLO_EXE)', '$(INPUT_EXE)' -DestinationPath '$(ZIP)' -Force"

copy: $(ZIP)
	copy /Y $(ZIP) $(DEST)

clean:
	if exist $(OUTDIR) rmdir /S /Q $(OUTDIR)

.PHONY: all copy clean
