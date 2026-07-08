package kr.co.iefriends.pcsx2;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Synchronous HTTP wrapper for the native HTTPDownloaderAndroid.
 *
 * <p>The native side runs each HTTPDownloader::Request on its own worker
 * thread (see HTTPDownloaderAndroid.cpp) and calls into {@link #doRequest}
 * blocking-style. This class doesn't manage queues or retries; it just
 * runs ONE HTTP transaction and returns the result. Caller is
 * responsible for thread management and following any redirects (we set
 * {@code setInstanceFollowRedirects(true)} which covers the rcheevos
 * server's behaviour).</p>
 *
 * <p>HttpURLConnection uses Android's system CA bundle and TLS stack —
 * no third-party dependencies, no shipping a CA file, no OpenSSL build.
 * Subject to the standard Android networking sandbox: requires
 * {@code <uses-permission android:name="android.permission.INTERNET"/>}
 * in the manifest and won't be reachable in airplane mode.</p>
 */
public final class HttpClient {

    private HttpClient() {}

    /** Plain holder so JNI can pull fields without hand-rolling a JNI
     *  signature for every accessor. Mirrors the C++ Response shape. */
    public static final class Response {
        public int statusCode;
        public String contentType = "";
        public byte[] data = new byte[0];
    }

    /**
     * Runs one synchronous HTTP request. Returns a {@link Response} —
     * the caller checks {@code statusCode} (negative on transport-level
     * failure: -1 generic error, -2 timeout, -3 cancelled — matches the
     * native HTTPDownloader sentinels).
     *
     * @param url       absolute URL.
     * @param method    "GET" or "POST".
     * @param postData  POST body bytes; ignored for GET.
     * @param userAgent value to send as User-Agent.
     * @param timeoutMs connect+read timeout per stage.
     */
    public static Response doRequest(
            String url,
            String method,
            byte[] postData,
            String userAgent,
            int timeoutMs) {
        Response out = new Response();
        HttpURLConnection conn = null;
        try {
            URL u = new URL(url);
            conn = (HttpURLConnection) u.openConnection();
            conn.setRequestMethod(method);
            conn.setConnectTimeout(timeoutMs);
            conn.setReadTimeout(timeoutMs);
            conn.setInstanceFollowRedirects(true);
            if (userAgent != null && !userAgent.isEmpty()) {
                conn.setRequestProperty("User-Agent", userAgent);
            }
            if ("POST".equalsIgnoreCase(method) && postData != null) {
                conn.setDoOutput(true);
                conn.setFixedLengthStreamingMode(postData.length);
                // RetroAchievements expects form-encoded payloads; the
                // Achievements layer hands us the body already-encoded so
                // we just need to set the content-type for it.
                conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(postData);
                }
            }

            // Some servers (or transport-level failures inside HUC) only
            // surface via getResponseCode() / getInputStream() — so we
            // wrap both in the try/catch.
            int code = conn.getResponseCode();
            out.statusCode = code;
            String ct = conn.getContentType();
            out.contentType = ct == null ? "" : ct;

            // 4xx/5xx still have an error body we want to forward to the
            // caller (e.g. rcheevos returns a JSON error blob with 200/4xx
            // depending on endpoint). Try the regular stream first; on
            // exception fall back to the error stream.
            InputStream in;
            try {
                in = conn.getInputStream();
            } catch (IOException ignored) {
                in = conn.getErrorStream();
            }
            if (in != null) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) {
                    baos.write(buf, 0, n);
                }
                out.data = baos.toByteArray();
                in.close();
            }
            return out;
        } catch (java.net.SocketTimeoutException e) {
            out.statusCode = -2; // HTTP_STATUS_TIMEOUT
            return out;
        } catch (Throwable e) {
            // Generic transport error — DNS failure, TLS handshake fail,
            // connection refused, malformed URL, etc.
            out.statusCode = -1; // HTTP_STATUS_ERROR
            return out;
        } finally {
            if (conn != null) conn.disconnect();
        }
    }
}
