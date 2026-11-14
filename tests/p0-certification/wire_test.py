import pathlib, subprocess, sys
android=pathlib.Path(sys.argv[1]); out=pathlib.Path(sys.argv[2]); suite=pathlib.Path(__file__).parent
src=android/'android/app/src/main/java/com/opentail/android'
subprocess.run(['java','-m','jdk.compiler/com.sun.tools.javac.Main','-d',str(out),str(src/'PanelPreviewProtocol.java'),str(src/'PanelV4LayoutProtocol.java'),str(suite/'WireInterop.java')],check=True)
subprocess.run(['java','-cp',str(out),'com.opentail.android.WireInterop',str(out)],check=True)
