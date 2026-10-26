import javax.tools.*;
import com.sun.source.util.JavacTask;
import java.util.*;

/** Syntax parsing ONLY. This does not validate Android API imports or compile an APK. */
public final class ParseJava {
    public static void main(String[] args) throws Exception {
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        if (compiler == null) throw new IllegalStateException("JDK javac unavailable");
        DiagnosticCollector<JavaFileObject> diagnostics = new DiagnosticCollector<>();
        try (StandardJavaFileManager manager = compiler.getStandardFileManager(diagnostics, null, null)) {
            Iterable<? extends JavaFileObject> units = manager.getJavaFileObjects(args);
            JavacTask parser = (JavacTask) compiler.getTask(null, manager, diagnostics, List.of("-proc:none"), null, units);
            parser.parse();
            boolean errors = false;
            for (Diagnostic<? extends JavaFileObject> d : diagnostics.getDiagnostics()) {
                if (d.getKind() == Diagnostic.Kind.ERROR) {
                    System.err.println(d);
                    errors = true;
                }
            }
            if (errors) System.exit(1);
            System.out.println("Java syntax parses (Android compilation not performed)");
        }
    }
}
