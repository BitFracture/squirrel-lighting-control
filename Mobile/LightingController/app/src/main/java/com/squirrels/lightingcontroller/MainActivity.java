package com.squirrels.lightingcontroller;

import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.support.v7.widget.LinearLayoutCompat;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.squirrels.lightingcontroller.NetInterface.RunnableWithData;

public class MainActivity extends AppCompatActivity {
    static int iCount = 0;

    final String strAutoEnabled = "M", strAutoDisable = "A";
    final String ERROR_DATA_NOT_SENT = "Error Sending Data";
    final String ERROR_NOT_CONNECTED = "Error Not Connected";
    public static TextView txtLog;
    NetInterface sock;

    public static void Print(final String s) {
        Handler postMessage = new Handler(Looper.getMainLooper());
        postMessage.post(new Runnable() {
            @Override
            public void run() {
                txtLog.setText(s + "\n" + txtLog.getText());
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setContentView(R.layout.activity_main);

        getComponentName();

        txtLog = (TextView) findViewById(R.id.txtLog);


        Print((savedInstanceState == null)? "True" : "False");
        Print("" + (iCount++));

        final TextView txtConnected = (TextView) findViewById(R.id.connectionStatus);
        txtConnected.setEnabled(false);
        NetInterface.onConnectedStateChange = new NetInterface.RunnableWithData() {
            @Override
            public void run() {
                if ("true".equals(data)) {
                    txtConnected.setText("Connected");
                    txtConnected.setTextColor(Color.BLACK);
                } else {
                    txtConnected.setText("Not connected");
                    txtConnected.setTextColor(Color.RED);
                }
            }
        };

        final SeekBar sbRed = (SeekBar) findViewById(R.id.redLevel);
        final SeekBar sbGreen = (SeekBar) findViewById(R.id.greenLevel);
        final SeekBar sbBlue = (SeekBar) findViewById(R.id.blueLevel);
        final ImageView imgSelected = (ImageView) findViewById(R.id.colorSelection);

        final SeekBar.OnSeekBarChangeListener sbar = new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int value, boolean b) {
                imgSelected.setBackgroundColor(Color.rgb(sbRed.getProgress(), sbGreen.getProgress(), sbBlue.getProgress()));
            }

            @Override
            public void onStartTrackingTouch(SeekBar var1) {}

            @Override
            public void onStopTrackingTouch(SeekBar var1) {
                NetInterface.sendAndRecv(new String("color " + sbRed.getProgress() + " " + sbGreen.getProgress() + " " + sbBlue.getProgress() + "\n"), new RunnableWithData<android.util.Pair<Boolean, String>>() {
                    @Override
                    public void run() {
                        if (data == null) {
                            Toast.makeText(getApplicationContext(),
                                    ERROR_NOT_CONNECTED,
                                    Toast.LENGTH_SHORT).show();

                        } else if (!data.first) {
                            Toast.makeText(getApplicationContext(),
                                    ERROR_DATA_NOT_SENT,
                                    Toast.LENGTH_SHORT).show();

                            MainActivity.Print("Error Sending Data");
                        } else {
                            if (data.second == null) {
                                MainActivity.Print("No Color Ack Recieved");
                            } else {
                                MainActivity.Print("Color Ack: " + data.toString());
                            }
                        }
                    }
                });
            }
        };

        sbRed.setOnSeekBarChangeListener(sbar);
        sbGreen.setOnSeekBarChangeListener(sbar);
        sbBlue.setOnSeekBarChangeListener(sbar);
        sbar.onProgressChanged(null, 0, false);

        final SeekBar tempLevel = (SeekBar) findViewById(R.id.tempLevel);
        tempLevel.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {}

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                sendTemperature(seekBar);
            }
        });

        final Button switchTempAuto = (Button) findViewById(R.id.btnSetTempAuto);
        switchTempAuto.setText(strAutoEnabled);
        switchTempAuto.setOnClickListener(new Button.OnClickListener() {
            public void onClick(final View v) {
                v.setEnabled(false);
                NetInterface.sendAndRecv(new String("temp auto\n"),
                        new RunnableWithData<android.util.Pair<Boolean, String>>() {
                            @Override
                            public void run() {
                                if (data == null) {
                                    Toast.makeText(getApplicationContext(),
                                            ERROR_NOT_CONNECTED,
                                            Toast.LENGTH_SHORT).show();

                                } else if (!data.first) {
                                    Toast.makeText(getApplicationContext(),
                                            ERROR_DATA_NOT_SENT,
                                            Toast.LENGTH_SHORT).show();

                                    MainActivity.Print("Error Sending Data");
                                } else {
                                    if (data.second == null) {
                                        MainActivity.Print("No Set Temp Auto Ack Received");
                                    } else {
                                        MainActivity.Print("Temp Auto Ack: " + data.toString());
                                    }
                                }

                                if (switchTempAuto.getText().equals(strAutoEnabled)) {
                                    switchTempAuto.setText(strAutoDisable);
                                    tempLevel.setEnabled(false);
                                } else {
                                    switchTempAuto.setText(strAutoEnabled);
                                    tempLevel.setEnabled(true);
                                    sendTemperature(tempLevel);
                                }
                                v.setEnabled(true);
                            }
                        });
            }
        });


        final Button switchColorAuto = (Button) findViewById(R.id.btnSetColorAuto);
        switchColorAuto.setText(strAutoEnabled);
        switchColorAuto.setOnClickListener(new Button.OnClickListener() {
            public void onClick(final View v) {
                v.setEnabled(false);
                NetInterface.sendAndRecv(new String("color auto\n"),
                        new RunnableWithData<android.util.Pair<Boolean, String>>() {
                            @Override
                            public void run() {
                                if (data == null) {
                                    Toast.makeText(getApplicationContext(),
                                            ERROR_NOT_CONNECTED,
                                            Toast.LENGTH_SHORT).show();

                                } else if (!data.first) {
                                    Toast.makeText(getApplicationContext(),
                                            ERROR_DATA_NOT_SENT,
                                            Toast.LENGTH_SHORT).show();

                                    MainActivity.Print("Error Sending Data");
                                } else {
                                    if (data.second == null) {
                                        MainActivity.Print("No Set Color Auto Ack Received");
                                    } else {
                                        MainActivity.Print("Color Auto Ack: " + data.toString());
                                    }
                                }

                                if (switchColorAuto.getText().equals(strAutoEnabled)) {
                                    switchColorAuto.setText(strAutoDisable);
                                    sbRed.setEnabled(false);
                                    sbGreen.setEnabled(false);
                                    sbBlue.setEnabled(false);
                                } else {
                                    switchColorAuto.setText(strAutoEnabled);
                                    sbRed.setEnabled(true);
                                    sbGreen.setEnabled(true);
                                    sbBlue.setEnabled(true);
                                    sbar.onStopTrackingTouch(sbRed);
                                }
                                v.setEnabled(true);
                            }
                        });
            }
        });

        // sbRed.setOnSeekBarChangeListener();
        NetInterface.onConnectedStateChange.data = NetInterface.isConnected() ? "true" : "false";
        NetInterface.onConnectedStateChange.run();
        super.onCreate(savedInstanceState);
    }

    void  sendTemperature(SeekBar seekBar) {
        NetInterface.sendAndRecv(new String("temp " + seekBar.getProgress() + "\n"),
                new RunnableWithData<android.util.Pair<Boolean, String>>() {
                    @Override
                    public void run() {
                        if (data == null) {
                            Toast.makeText(getApplicationContext(),
                                    ERROR_NOT_CONNECTED,
                                    Toast.LENGTH_SHORT).show();

                        } else if (!data.first) {
                            Toast.makeText(getApplicationContext(),
                                    ERROR_DATA_NOT_SENT,
                                    Toast.LENGTH_SHORT).show();

                            MainActivity.Print("Error Sending Data");
                        } else {
                            if (data.second == null) {
                                MainActivity.Print("No Temp Ack Recieved");
                            } else {
                                MainActivity.Print("Temp Ack: " + data.toString());
                            }
                        }
                    }
                });
    }

    /**
     * Overites hexSymbolCount number of bytes of the given char aray, strLastChar.
     * The indices replaced are strLastChar[0] to writeStart[1 - hexSymbolCount]
     * inclusive of both.
     */
    public void numToHexStr(int num, char[] strOutput, int fistCharIndex, int hexSymbolCount) {
        final char[] HEX_LOOKUP_TBL = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        for (hexSymbolCount -= 1; hexSymbolCount >= 0; hexSymbolCount -= 1) {
            strOutput[fistCharIndex + hexSymbolCount] = HEX_LOOKUP_TBL[num & 0xf];
            num >>= 4;
        }
    }
}
