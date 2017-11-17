package com.squirrels.lightingcontroller;

import android.os.AsyncTask;
import android.os.Handler;
import android.os.Looper;

import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.UnknownHostException;
import java.util.Calendar;
import java.util.Date;
import android.content.Context;
import android.widget.TextView;
import android.util.Pair;

import static java.lang.Thread.sleep;

public class NetInterface {
    public static RunnableWithData onConnectedStateChange = null;
    private static volatile Object readWriteLock = new Object();
    private InetSocketAddress ioController;
    private static NetInterface sInstance;
    private boolean isConnected = false;
    private OutputStream oStream;
    private InputStream iStream;
    private Socket sock = null;

    static {
        sInstance = new NetInterface("192.168.3.1", 23);
    }

    public static boolean isConnected() {
        return sInstance.isConnected;
    }

    public static void sendAndRecv(final String msg, final RunnableWithData<Pair<Boolean, String>> callback) {
        (new Thread() {
            public void run() {
                Handler mainHandler = new Handler(Looper.getMainLooper());

                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
                Date currentTime = Calendar.getInstance().getTime();
                callback.timeStamp = Calendar.getInstance().getTime();

                if (sInstance.isConnected) {
                    callback.data =  sendAndRecieve(msg);
                }

                Runnable myRunnable = new Runnable() {
                    @Override
                    public void run() {
                        callback.run();
                    }
                };

                mainHandler.post(myRunnable);
            }
        }).start();
    }

    public static Pair sendAndRecieve(String toSend) {
        Pair<Boolean, String> rtn = null;
        synchronized (readWriteLock) {
            if (sInstance.sendMessage(toSend)) {
                rtn = new Pair<>(Boolean.TRUE, sInstance.recvMessage());
            } else {
                rtn = new Pair<>(Boolean.FALSE, null);
            }
        }

        return rtn;
    }

    public static abstract class RunnableWithData<T> implements Runnable {
        public Date timeStamp;
        public T data;
    }

    private NetInterface(String remoteAddress, int remotePort) {
        try {
            ioController = new InetSocketAddress(InetAddress.getByName(remoteAddress), remotePort);
        } catch (UnknownHostException e) {}

        (new Thread(new Runnable() {
            @Override
            public void run() {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
                while (true) {
                    if (!isConnected) {
                        connect();
                        try {
                            sleep(50);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    } else {
                        try {
                            sleep(1000);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }

                        checkIfConnected();
                    }
                }
            }
        })).start();
    }

    private void checkIfConnected() {
//        if (sock == null || sock.isClosed() || !sock.isConnected() || !("ack".equals(sendAndRecieve("sin").second))) {
        if (sock == null || sock.isClosed() || !sock.isConnected()) {
            if (isConnected != false) {
                updateConnectedState(false);
            }
        }
    }

    private boolean sendMessage(String a) {
        try {
            oStream.write(a.getBytes());
            oStream.flush();
            return true;
        } catch (IOException e) {
            updateConnectedState(false);
            // return false;
        }

        return false;
    }

    private String recvMessage() {
        String str = null;
        int bytesRead = 0;
        byte[] buffer = new byte[500];

        try {
            bytesRead = iStream.read(buffer, 0, buffer.length);
            if (bytesRead >= 0) {
                str = new String(buffer, 0, bytesRead);
                MainActivity.Print(str);
            }
        } catch (IOException e) {
            updateConnectedState(false);
            // Do nothing
        }

        return str;
    }

    private boolean connect() {
        if (sock != null) return true;
        boolean successCode = false;

        try {
            MainActivity.Print("Conecting to: " + ioController.getAddress() + ":" + ioController.getPort());
            sock = new Socket();
            sock.connect(ioController, 1000);
            sock.setTcpNoDelay(true);
            sock.setSoTimeout(1000);
            MainActivity.Print("3.5");
            iStream = sock.getInputStream();
            oStream = sock.getOutputStream();


            MainActivity.Print("4");
            String serverOut = recvMessage();
            if (serverOut != null) serverOut = serverOut.trim();
            if ("mode".equals(serverOut)) {
                if (sendMessage("persist")) {
                    MainActivity.Print("Successfully set to persist");
                } else {
                    MainActivity.Print("Not able to set as persistent");
                }
            } else {
                MainActivity.Print("Server Unknown Command: \"" + serverOut + "\"");
            }

            MainActivity.Print("5");
            serverOut = recvMessage();
            if (serverOut != null) serverOut = serverOut.trim();

            if ("identify".equals(serverOut)) {
                if (sendMessage("mobile")) {
                    MainActivity.Print("Successfully set to mobile");
                    MainActivity.Print("Successfully connected!");
                    successCode = true;
                } else {
                    MainActivity.Print("Not able to identify");
                }
            } else {
                MainActivity.Print("Identify Unknown Command: \"" + serverOut + "\"");
            }

            if (!successCode) sock = null;

        } catch (IOException e) {
            MainActivity.Print("Unable to create socket: IOException > " + e);
            iStream = null;
            oStream = null;
            sock = null;
        } catch (Exception e) {
            MainActivity.Print("Unable to catch exception: " + e);
            if (sock != null) {
                try {
                    sock.close();
                } catch (IOException e1) {
                    e1.printStackTrace();
                }
            }
            sock = null;
        }

         updateConnectedState(successCode);

        return successCode;
    }

    private void updateConnectedState(boolean newState) {
        if (newState != isConnected) {
            isConnected = newState;

            if (newState == false) {
                if (sock != null) {
                    try {
                        sock.close();
                    } catch (IOException e) {
                    } finally {
                        sock = null;
                    }
                }
            }
            if (onConnectedStateChange != null) {
                Date currentTime = Calendar.getInstance().getTime();
                onConnectedStateChange.timeStamp = currentTime;
                onConnectedStateChange.data = "" + newState;

                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        onConnectedStateChange.run();
                    }
                });
            }
        }
    }
}
