package kr.co.iefriends.pcsx2.provider;

import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.webkit.MimeTypeMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Arrays;
import java.util.Comparator;

import kr.co.iefriends.pcsx2.utils.DataDirectoryManager;

/**
 * DocumentsProvider exposing the ARMSX2 data directory through the Storage Access Framework.
 */
public class Armsx2DocumentsProvider extends DocumentsProvider {
    public static final String AUTHORITY_SUFFIX = ".documents";
    private static final String ROOT_ID = "armsx2-root";
    private static final String DOC_ID_PREFIX = "path:";
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_FLAGS,
            Root.COLUMN_AVAILABLE_BYTES,
            Root.COLUMN_CAPACITY_BYTES,
            Root.COLUMN_MIME_TYPES
    };
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_SIZE,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS
    };

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor queryRoots(@Nullable String[] projection) throws FileNotFoundException {
        Context context = getContext();
        if (context == null) {
            throw new FileNotFoundException("Context unavailable");
        }
        File base = requireBaseDir(context);
        String docId = toDocumentId(base);

        MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_ROOT_PROJECTION);
        MatrixCursor.RowBuilder row = result.newRow();
        row.add(Root.COLUMN_ROOT_ID, ROOT_ID);
        row.add(Root.COLUMN_DOCUMENT_ID, docId);
        row.add(Root.COLUMN_TITLE, context.getString(kr.co.iefriends.pcsx2.R.string.app_name));
        row.add(Root.COLUMN_SUMMARY, context.getString(kr.co.iefriends.pcsx2.R.string.documents_provider_root_summary));
        row.add(Root.COLUMN_FLAGS, Root.FLAG_LOCAL_ONLY | Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_IS_CHILD);
        row.add(Root.COLUMN_AVAILABLE_BYTES, base.getUsableSpace());
        row.add(Root.COLUMN_CAPACITY_BYTES, base.getTotalSpace());
        row.add(Root.COLUMN_MIME_TYPES, "*/*");
        return result;
    }

    @Override
    public Cursor queryDocument(@NonNull String documentId, @Nullable String[] projection) throws FileNotFoundException {
        File file = getFileForDocumentId(documentId);
        MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        includeDocument(result, documentId, file);
        return result;
    }

    @Override
    public Cursor queryChildDocuments(@NonNull String parentDocumentId, @Nullable String[] projection, @Nullable String sortOrder) throws FileNotFoundException {
        File parent = getFileForDocumentId(parentDocumentId);
        if (!parent.isDirectory()) {
            throw new FileNotFoundException("Not a directory: " + parent);
        }
        File[] children = parent.listFiles();
        MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        if (children == null || children.length == 0) {
            return result;
        }
        Arrays.sort(children, Comparator.comparing(File::getName, String.CASE_INSENSITIVE_ORDER));
        for (File child : children) {
            if (child == null || !child.exists()) {
                continue;
            }
            if (isHiddenHelper(child.getName())) {
                continue;
            }
            String childId = toDocumentId(child);
            includeDocument(result, childId, child);
        }
        return result;
    }

    @Override
    public ParcelFileDescriptor openDocument(@NonNull String documentId, @NonNull String mode, @Nullable CancellationSignal signal) throws FileNotFoundException {
        File file = getFileForDocumentId(documentId);
        int modeBits = modeToModeFlags(mode);
        return ParcelFileDescriptor.open(file, modeBits);
    }

    @Override
    public String createDocument(@NonNull String parentDocumentId, @NonNull String mimeType, @NonNull String displayName) throws FileNotFoundException {
        File parent = getFileForDocumentId(parentDocumentId);
        if (!parent.isDirectory()) {
            throw new FileNotFoundException("Parent is not a directory: " + parent);
        }
        String sanitized = sanitizeDisplayName(displayName);
        File target = new File(parent, sanitized);
        target = ensureUniqueName(target, mimeType);
        try {
            if (Document.MIME_TYPE_DIR.equals(mimeType)) {
                if (!target.mkdirs() && !target.isDirectory()) {
                    throw new FileNotFoundException("Unable to create directory " + target);
                }
            } else {
                if (!target.exists() && !target.createNewFile()) {
                    throw new FileNotFoundException("Unable to create file " + target);
                }
            }
        } catch (IOException e) {
            throw new FileNotFoundException("Unable to create document: " + e.getMessage());
        }
        return toDocumentId(target);
    }

    @Override
    public void deleteDocument(@NonNull String documentId) throws FileNotFoundException {
        File target = getFileForDocumentId(documentId);
        if (!deleteRecursively(target)) {
            throw new FileNotFoundException("Unable to delete " + target);
        }
    }

    @Override
    public String renameDocument(@NonNull String documentId, @NonNull String displayName) throws FileNotFoundException {
        File target = getFileForDocumentId(documentId);
        File parent = target.getParentFile();
        if (parent == null) {
            throw new FileNotFoundException("Cannot rename root");
        }
        File renamed = new File(parent, sanitizeDisplayName(displayName));
        renamed = ensureSequentialRename(target, renamed);
        if (!target.renameTo(renamed)) {
            throw new FileNotFoundException("Unable to rename " + target + " to " + renamed);
        }
        return toDocumentId(renamed);
    }

    @Override
    public boolean isChildDocument(@NonNull String parentDocumentId, @NonNull String documentId) {
        try {
            File parent = getFileForDocumentId(parentDocumentId);
            File child = getFileForDocumentId(documentId);
            return isDescendant(parent, child);
        } catch (FileNotFoundException e) {
            return false;
        }
    }

    @Override
    public String getDocumentType(@NonNull String documentId) throws FileNotFoundException {
        File file = getFileForDocumentId(documentId);
        return resolveMimeType(file);
    }

    @NonNull
    public static String authorityFor(@NonNull Context context) {
        return context.getPackageName() + AUTHORITY_SUFFIX;
    }

    @NonNull
    public static Uri buildRootDocumentUri(@NonNull Context context) throws FileNotFoundException {
        File base = requireBaseDir(context);
        String docId = toDocumentId(base);
        return DocumentsContract.buildDocumentUri(authorityFor(context), docId);
    }

    @NonNull
    public static Uri buildRootUri(@NonNull Context context) {
        return DocumentsContract.buildRootUri(authorityFor(context), ROOT_ID);
    }

    private File getFileForDocumentId(@NonNull String documentId) throws FileNotFoundException {
        Context context = getContext();
        if (context == null) {
            throw new FileNotFoundException("Context unavailable");
        }
        File base = requireBaseDir(context);
        if (!documentId.startsWith(DOC_ID_PREFIX)) {
            throw new FileNotFoundException("Unknown document id: " + documentId);
        }
        String path = documentId.substring(DOC_ID_PREFIX.length());
        File target = new File(path);
        ensureDescendant(base, target);
        return target;
    }

    private static File requireBaseDir(@NonNull Context context) throws FileNotFoundException {
        File dir = DataDirectoryManager.getDataRoot(context.getApplicationContext());
        if (dir == null) {
            throw new FileNotFoundException("Data directory unavailable");
        }
        if (!dir.exists() && !dir.mkdirs()) {
            throw new FileNotFoundException("Unable to create data directory");
        }
        return dir;
    }

    private static void includeDocument(@NonNull MatrixCursor cursor, @NonNull String docId, @NonNull File file) throws FileNotFoundException {
        MatrixCursor.RowBuilder row = cursor.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, docId);
        row.add(Document.COLUMN_DISPLAY_NAME, displayName(file));
        row.add(Document.COLUMN_MIME_TYPE, resolveMimeType(file));
        row.add(Document.COLUMN_SIZE, file.isFile() ? file.length() : null);
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, documentFlags(file));
    }

    private static String displayName(File file) throws FileNotFoundException {
        String name = file.getName();
        if (name == null || name.isEmpty()) {
            try {
                name = file.getCanonicalFile().getName();
            } catch (IOException e) {
                throw new FileNotFoundException("Unable to resolve display name: " + e.getMessage());
            }
        }
        if (name == null || name.isEmpty()) {
            name = file.getAbsolutePath();
        }
        return name;
    }

    private static int documentFlags(File file) {
        int flags = 0;
        if (file.isDirectory()) {
            flags |= Document.FLAG_DIR_SUPPORTS_CREATE;
            flags |= Document.FLAG_SUPPORTS_DELETE;
            flags |= Document.FLAG_SUPPORTS_WRITE;
            flags |= Document.FLAG_SUPPORTS_RENAME;
            flags |= Document.FLAG_DIR_PREFERS_GRID;
        } else {
            flags |= Document.FLAG_SUPPORTS_DELETE;
            flags |= Document.FLAG_SUPPORTS_WRITE;
            flags |= Document.FLAG_SUPPORTS_RENAME;
        }
        return flags;
    }

    private static String resolveMimeType(File file) {
        if (file.isDirectory()) {
            return Document.MIME_TYPE_DIR;
        }
        String name = file.getName();
        int lastDot = name.lastIndexOf('.');
        if (lastDot >= 0 && lastDot < name.length() - 1) {
            String ext = name.substring(lastDot + 1).toLowerCase();
            String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext);
            if (mime != null) {
                return mime;
            }
        }
        return "application/octet-stream";
    }

    private static String sanitizeDisplayName(String name) {
        String sanitized = name.replaceAll("[\\\\/:*?\"<>|]", "_").trim();
        return sanitized.isEmpty() ? "new_file" : sanitized;
    }

    private static File ensureUniqueName(File target, String mimeType) {
        if (!target.exists()) {
            return target;
        }
        String baseName = target.getName();
        String suffix = "";
        String prefix = baseName;
        int dot = baseName.lastIndexOf('.');
        if (dot > 0 && !Document.MIME_TYPE_DIR.equals(mimeType)) {
            prefix = baseName.substring(0, dot);
            suffix = baseName.substring(dot);
        }
        File parent = target.getParentFile();
        int index = 1;
        File candidate;
        do {
            candidate = new File(parent, prefix + " (" + index + ")" + suffix);
            index++;
        } while (candidate.exists());
        return candidate;
    }

    private static File ensureSequentialRename(File original, File desired) {
        if (!desired.exists()) {
            return desired;
        }
        File parent = desired.getParentFile();
        String name = desired.getName();
        String suffix = "";
        String prefix = name;
        int dot = name.lastIndexOf('.');
        if (dot > 0 && original.isFile()) {
            prefix = name.substring(0, dot);
            suffix = name.substring(dot);
        }
        int index = 1;
        File candidate;
        do {
            candidate = new File(parent, prefix + " (" + index + ")" + suffix);
            index++;
        } while (candidate.exists());
        return candidate;
    }

    private static boolean deleteRecursively(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    if (!deleteRecursively(child)) {
                        return false;
                    }
                }
            }
        }
        return file.delete();
    }

    private static int modeToModeFlags(String mode) throws FileNotFoundException {
        switch (mode) {
            case "r":
                return ParcelFileDescriptor.MODE_READ_ONLY;
            case "w":
            case "wt":
                return ParcelFileDescriptor.MODE_WRITE_ONLY
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_TRUNCATE;
            case "wa":
                return ParcelFileDescriptor.MODE_WRITE_ONLY
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_APPEND;
            case "rw":
                return ParcelFileDescriptor.MODE_READ_WRITE;
            case "rwt":
                return ParcelFileDescriptor.MODE_READ_WRITE
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_TRUNCATE;
            default:
                throw new FileNotFoundException("Invalid mode: " + mode);
        }
    }

    private static boolean isDescendant(File parent, File child) throws FileNotFoundException {
        try {
            String parentPath = parent.getCanonicalPath();
            String childPath = child.getCanonicalPath();
            return childPath.equals(parentPath) || childPath.startsWith(parentPath + File.separator);
        } catch (IOException e) {
            throw new FileNotFoundException("Unable to resolve canonical path: " + e.getMessage());
        }
    }

    private static void ensureDescendant(File root, File target) throws FileNotFoundException {
        if (!isDescendant(root, target)) {
            throw new FileNotFoundException("Access outside data directory is not allowed");
        }
    }

    private static String toDocumentId(File file) throws FileNotFoundException {
        try {
            return DOC_ID_PREFIX + file.getCanonicalPath();
        } catch (IOException e) {
            throw new FileNotFoundException("Unable to resolve canonical path: " + e.getMessage());
        }
    }

    private static boolean isHiddenHelper(String name) {
        return ".armsx2_write_probe".equals(name);
    }
}
