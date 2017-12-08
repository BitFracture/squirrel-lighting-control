package com.squirrels.lightingcontroller;

import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.Debug;
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

import static com.squirrels.lightingcontroller.R.id.btnClapIsOn;
import static com.squirrels.lightingcontroller.R.id.btnIsOn;
import static com.squirrels.lightingcontroller.R.id.btnListenIsOn;
import static com.squirrels.lightingcontroller.R.id.btnSetMotionOn;
import static com.squirrels.lightingcontroller.R.id.motionLevel;

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
        final Button btnOn = (Button) findViewById(btnIsOn);
        btnOn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                btnOn.setEnabled(false);
                MainActivity.Print("power " + (!btnOn.isActivated() ? "off" : "on"));
                NetInterface.sendAndRecv(new String("power " + (!btnOn.isActivated() ? "off" : "on") + "\n"),
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
                                    MainActivity.Print("No Set Power Auto Ack Received");
                                } else {
                                    MainActivity.Print("Power Auto Ack: " + data.toString());
                                }
                            }

                            btnOn.setEnabled(true);
                        }
                    });
            }
        });

        final Button btnClapOn = (Button) findViewById(btnClapIsOn);
        btnClapOn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                btnClapOn.setEnabled(false);
                MainActivity.Print("clap " + (!btnClapOn.isActivated() ? "on" : "off"));
                NetInterface.sendAndRecv(new String("clap " + (!btnClapOn.isActivated() ? "on" : "off") + "\n"),
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
                                        MainActivity.Print("No Set Clap Auto Ack Received");
                                    } else {
                                        MainActivity.Print("Clap Auto Ack: " + data.toString());
                                    }
                                }

                                btnClapOn.setEnabled(true);
                            }
                        });
            }
        });

        final Button btnListenOn = (Button) findViewById(btnListenIsOn);
        btnListenOn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                btnListenOn.setEnabled(false);
                MainActivity.Print("listen");
                NetInterface.sendAndRecv(new String("listen\n"),
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
                                        MainActivity.Print("No Set Listen Ack Received");
                                    } else {
                                        MainActivity.Print("Listen Ack: " + data.toString());
                                    }
                                }

                                btnOn.setEnabled(true);
                            }
                        });
            }
        });

        final TextView txtConnected = (TextView) findViewById(R.id.connectionStatus);
        txtConnected.setEnabled(false);
        NetInterface.onConnectedStateChange = new NetInterface.RunnableWithData() {
            @Override
            public void run() {
                if ("true".equals(data)) {
                    txtConnected.setText("Connected");
                    txtConnected.setTextColor(Color.BLACK);
                    btnOn.setEnabled(true);
                } else {
                    txtConnected.setText("Not connected");
                    txtConnected.setTextColor(Color.RED);
                    btnOn.setText("ON");
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
                MainActivity.Print("color " + sbRed.getProgress() + " " + sbGreen.getProgress() + " " + sbBlue.getProgress());
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
                MainActivity.Print("temp auto");
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
                                }
                                v.setEnabled(true);
                            }
                        });
            }
        });
//        sendMotion
        final Button switchMotionOn = (Button) findViewById(R.id.btnSetMotionOn);
        switchMotionOn.setText(strAutoEnabled);
        switchMotionOn.setOnClickListener(new Button.OnClickListener() {
            public void onClick(final View v) {
                v.setEnabled(false);
                MainActivity.Print("motion " + (switchMotionOn.isActivated()? "off" : "on"));
                NetInterface.sendAndRecv(new String("motion " + (switchMotionOn.isActivated()? "off" : "on") + "\n"),
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

                                if (switchMotionOn.getText().equals(strAutoEnabled)) {
                                    switchMotionOn.setText(strAutoDisable);
                                } else {
                                    switchMotionOn.setText(strAutoEnabled);
                                }
                                v.setEnabled(true);
                            }
                        });
            }
        });

        final SeekBar motionLevel = (SeekBar) findViewById(R.id.motionLevel);
        motionLevel.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {}

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                MainActivity.Print("motion " + seekBar.getProgress());
                NetInterface.sendAndRecv(new String("motion " + seekBar.getProgress() + "\n"),
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
                                    switchMotionOn.setActivated(false);
                                    if (data.second == null) {
                                        MainActivity.Print("No Motion Ack Recieved");
                                    } else {
                                        MainActivity.Print("Motion Ack: " + data.toString());
                                    }
                                }
                            }
                        });
            }
        });

        final Button switchColorAuto = (Button) findViewById(R.id.btnSetColorAuto);
        switchColorAuto.setText(strAutoEnabled);
        switchColorAuto.setOnClickListener(new Button.OnClickListener() {
            public void onClick(final View v) {
                v.setEnabled(false);
                MainActivity.Print("color auto");
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


        final Button switchBrightnessAuto = (Button) findViewById(R.id.btnSetBrightnessOn);
        switchBrightnessAuto.setText(strAutoEnabled);
        switchBrightnessAuto.setOnClickListener(new Button.OnClickListener() {
            public void onClick(final View v) {
                v.setEnabled(false);
                MainActivity.Print("brightness auto");
                NetInterface.sendAndRecv(new String("brightness auto\n"),
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

                                if (switchBrightnessAuto.getText().equals(strAutoEnabled)) {
                                    switchBrightnessAuto.setText(strAutoDisable);
                                    sbRed.setEnabled(false);
                                    sbGreen.setEnabled(false);
                                    sbBlue.setEnabled(false);
                                } else {
                                    switchBrightnessAuto.setText(strAutoEnabled);
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

        final SeekBar brightnesLevel = (SeekBar) findViewById(R.id.brightnessLevel);
        motionLevel.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {}

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                MainActivity.Print("brightness " + seekBar.getProgress());
                NetInterface.sendAndRecv(new String("brightness " + seekBar.getProgress() + "\n"),
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
                                    switchMotionOn.setActivated(false);
                                    if (data.second == null) {
                                        MainActivity.Print("No Brightness Ack Recieved");
                                    } else {
                                        MainActivity.Print("Brightness Ack: " + data.toString());
                                    }
                                }
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
        MainActivity.Print("temp " + seekBar.getProgress());
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
