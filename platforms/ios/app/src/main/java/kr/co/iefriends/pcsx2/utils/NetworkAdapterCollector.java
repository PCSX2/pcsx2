package kr.co.iefriends.pcsx2.utils;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.RouteInfo;
import android.provider.Settings;
import android.util.Log;
import android.net.IpPrefix;

import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;

import kr.co.iefriends.pcsx2.App;

public class NetworkAdapterCollector {
    public static class AdapterInfo {
        public String name;
        public String displayName;
        public boolean isUp;
        public boolean isLoopback;
        public boolean isVirtual;
        public boolean supportsMulticast;
        public int mtu;
        public String[] ipAddresses;
        public String[] dnsServers;
        public RouteInfo[] routes; // structured routes

        public static class RouteInfo {
            public String destination;    // e.g. "10.80.129.236/30"
            public String address;        // network address only, e.g. "10.80.129.236"
            public int prefix;            // prefix length
            public boolean isIPv6;

            public String gateway;        // gateway IP or "none"
            public boolean hasGateway;
            public boolean isDefault;
            public boolean isHostRoute;
            public boolean isNetworkRoute;
            public boolean isDirect;

            // Address classification flags
            public boolean isAnyLocal;
            public boolean isSiteLocal;
            public boolean isLoopback;
            public boolean isLinkLocal;
            public boolean isMulticast;
        }
    }

    private static final String TAG = "NetworkAdapterCollector";

    // JNI callback: must be implemented in C++
    public static native void onAdaptersCollected(AdapterInfo[] adapters);

    public static void collectAdapters() {
        List<AdapterInfo> resultAdapters = new ArrayList<>();

        try {
            Context context = App.getContext();

            ConnectivityManager cm = context.getSystemService(ConnectivityManager.class);
            Network[] networks = cm.getAllNetworks();

            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();

            for (NetworkInterface ni : Collections.list(interfaces)) {
                AdapterInfo adapter = new AdapterInfo();
                adapter.name = ni.getName();
                adapter.displayName = ni.getDisplayName();
                adapter.isUp = ni.isUp();
                adapter.isLoopback = ni.isLoopback();
                adapter.isVirtual = ni.isVirtual();
                adapter.supportsMulticast = ni.supportsMulticast();

                // IP addresses
                List<String> ips = new ArrayList<>();
                for (InetAddress addr : Collections.list(ni.getInetAddresses())) {
                    ips.add(addr.getHostAddress());
                }
                adapter.ipAddresses = ips.toArray(new String[0]);

                for (Network net : networks) {
                    LinkProperties lp = cm.getLinkProperties(net);
                    if (lp == null) continue;

                    // lp.getInterfaceName() matches NetworkInterface.getName()
                    if (!ni.getName().equals(lp.getInterfaceName()))
                        continue;

                    adapter.mtu = lp.getMtu();

                    // DNS servers
                    List<String> dnsList = new ArrayList<>();
                    for (InetAddress dns : lp.getDnsServers()) {
                        dnsList.add(dns.getHostAddress());
                    }
                    adapter.dnsServers = dnsList.toArray(new String[0]);

                    // Routes
                    List<AdapterInfo.RouteInfo> routeObjects = new ArrayList<>();
                    for (RouteInfo route : lp.getRoutes()) {

                        AdapterInfo.RouteInfo rid = new AdapterInfo.RouteInfo();

                        IpPrefix dst = route.getDestination();
                        InetAddress gw = route.getGateway();

                        // --- Destination ---
                        rid.destination     = dst != null ? dst.toString() : "none";
                        rid.address         = (dst != null && dst.getAddress() != null)
                                ? dst.getAddress().getHostAddress()
                                : "none";
                        rid.prefix          = dst != null ? dst.getPrefixLength() : -1;

                        // --- IP family ---
                        rid.isIPv6          = (dst != null && dst.getAddress() instanceof Inet6Address);

                        // --- Gateway ---
                        rid.gateway         = gw != null ? gw.getHostAddress() : "none";
                        rid.hasGateway      = (gw != null);

                        // --- Basic flags ---
                        rid.isDefault       = route.isDefaultRoute();
                        rid.isHostRoute     = (rid.prefix == (rid.isIPv6 ? 128 : 32));
                        rid.isNetworkRoute  = !rid.isHostRoute && !rid.isDefault;
                        rid.isDirect        = (gw == null);

                        // --- Address characteristics ---
                        InetAddress dstAddr = dst != null ? dst.getAddress() : null;
                        rid.isAnyLocal      = dstAddr != null && dstAddr.isAnyLocalAddress();
                        rid.isSiteLocal     = dstAddr != null && dstAddr.isSiteLocalAddress();
                        rid.isLoopback      = dstAddr != null && dstAddr.isLoopbackAddress();
                        rid.isLinkLocal     = dstAddr != null && dstAddr.isLinkLocalAddress();
                        rid.isMulticast     = dstAddr != null && dstAddr.isMulticastAddress();

                        routeObjects.add(rid);
                    }

                    adapter.routes = routeObjects.toArray(new AdapterInfo.RouteInfo[0]);
                }

                resultAdapters.add(adapter);
            }

            // Pass adapters to native C++
            onAdaptersCollected(resultAdapters.toArray(new AdapterInfo[0]));

        } catch (Exception e) {
            Log.e(TAG, "Error enumerating network interfaces", e);
        }
    }

    public static String generateUniqueMac() {
        String androidId = Settings.Secure.getString(
                App.getContext().getContentResolver(),
                Settings.Secure.ANDROID_ID
        );

        if (androidId == null)
            androidId = "0000000000000000";

        try {
            MessageDigest digest = MessageDigest.getInstance("MD5");
            byte[] hash = digest.digest(androidId.getBytes());
            StringBuilder sb = new StringBuilder(12);

            for (int i = 0; i < 6; i++) {
                sb.append(String.format("%02X", hash[i]));
                if (i != 5) sb.append(":");
            }

            return sb.toString();
        } catch (Exception e) {
            return "00:00:00:00:00:00";
        }
    }

    public static void listNetworkAdapters() {
        try {
            Context context = App.getContext();

            ConnectivityManager cm = context.getSystemService(ConnectivityManager.class);
            Network[] networks = cm.getAllNetworks();

            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();

            for (NetworkInterface ni : Collections.list(interfaces)) {
                Log.d(TAG, "=====================================");
                Log.d(TAG, ni.getName() + ":");
                Log.d(TAG, "    Up: " + ni.isUp());
                Log.d(TAG, "    Loopback: " + ni.isLoopback());
                Log.d(TAG, "    Virtual: " + ni.isVirtual());
                Log.d(TAG, "    Supports Multicast: " + ni.supportsMulticast());

                // IP addresses attached to this interface
                Log.d(TAG, "    Address:");
                for (InetAddress addr : Collections.list(ni.getInetAddresses())) {
                    Log.d(TAG, "        " + addr.getHostAddress());
                }

                for (Network net : networks) {
                    LinkProperties lp = cm.getLinkProperties(net);
                    if (lp == null) continue;

                    // lp.getInterfaceName() matches NetworkInterface.getName()
                    if (!ni.getName().equals(lp.getInterfaceName()))
                        continue;

                    // MTU
                    int mtu = lp.getMtu();
                    Log.d(TAG, "    MTU: " + mtu);

                    // DNS Servers
                    List<InetAddress> dnsServers = lp.getDnsServers();
                    Log.d(TAG, "    DNS servers (" + dnsServers.size() + "):");
                    for (InetAddress dns : dnsServers) {
                        Log.d(TAG, "        " + dns.getHostAddress());
                    }

                    // Routes
                    List<RouteInfo> routes = lp.getRoutes();
                    Log.d(TAG, "    Routes (" + routes.size() + "):");
                    for (RouteInfo route : routes) {
                        IpPrefix dst = route.getDestination();
                        InetAddress gw = route.getGateway();

                        // Destination
                        String destination = dst.toString();
                        String address = dst.getAddress().getHostAddress();
                        int prefix = dst.getPrefixLength();

                        // IP family
                        boolean isIPv6 = dst.getAddress() instanceof Inet6Address;

                        // Gateway
                        String gateway = gw != null ? gw.getHostAddress() : "none";
                        boolean hasGateway = gw != null;

                        // Flags / types
                        boolean isDefault = route.isDefaultRoute();
                        boolean isHostRoute = (prefix == (isIPv6 ? 128 : 32));
                        boolean isNetworkRoute = !isHostRoute && !isDefault;
                        boolean isDirect = (gw == null);

                        InetAddress dstAddr = dst != null ? dst.getAddress() : null;
                        boolean isAnyLocal  = dstAddr != null && dstAddr.isAnyLocalAddress();
                        boolean isSiteLocal = dstAddr != null && dstAddr.isSiteLocalAddress();
                        boolean isLoopback  = dstAddr != null && dstAddr.isLoopbackAddress();
                        boolean isLinkLocal = dstAddr != null && dstAddr.isLinkLocalAddress();
                        boolean isMulticast = dstAddr != null && dstAddr.isMulticastAddress();

                        Log.d(TAG, "        " + route.toString() + ": ");
                        Log.d(TAG, "            Destination: " + destination);
                        Log.d(TAG, "            Address: " + address);
                        Log.d(TAG, "            Prefix: " + prefix);
                        Log.d(TAG, "            isIPv6: " + isIPv6);
                        Log.d(TAG, "            hasGateway: " + hasGateway);
                        Log.d(TAG, "            Gateway: " + gateway);
                        Log.d(TAG, "            isDefault: " + isDefault);
                        Log.d(TAG, "            isHostRoute: " + isHostRoute);
                        Log.d(TAG, "            isNetworkRoute: " + isNetworkRoute);
                        Log.d(TAG, "            isDirect: " + isDirect);
                        Log.d(TAG, "            Interface MTU: " + mtu);
                        Log.d(TAG, "            isAnyLocal: " + isAnyLocal);
                        Log.d(TAG, "            isSiteLocal: " + isSiteLocal);
                        Log.d(TAG, "            isLoopback: " + isLoopback);
                        Log.d(TAG, "            isLinkLocal: " + isLinkLocal);
                        Log.d(TAG, "            isMulticast: " + isMulticast);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error enumerating interfaces", e);
        }
    }

}
