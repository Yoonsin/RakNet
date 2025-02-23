package com.example.irrlichtdemoCP;

import androidx.appcompat.app.AppCompatActivity;

import android.app.NativeActivity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends NativeActivity {

    static {
        // Please add other libraries used by this activity.
        System.loadLibrary("IrrlichtDemo");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(stringFromJNI());
    }

    public native String stringFromJNI();
}
