package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.widget.Toast;

import com.gstreamer.GStreamer;

public class Tutorial3 extends Activity implements SurfaceHolder.Callback {
    private native void nativeInit(); // Initialize native code, build pipeline, etc
    private native void nativeFinalize(); // Destroy pipeline and shutdown native code
    private native void nativePlay(); // Set pipeline to PLAYING
    private native void nativePause(); // Set pipeline to PAUSED
    private static native boolean nativeClassInit(); // Initialize native class: cache Method IDs for callbacks
    private native void nativeSurfaceInit(Object surface);
    private native void nativeSurfaceFinalize();
    private long native_custom_data; // Native code will use this to keep private data
    private long video_receive_native_custom_data;
    private long audio_send_native_custom_data;

    private boolean is_playing_desired; // Whether the user asked to go to PLAYING

    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Initialize GStreamer and warn if it fails
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.main);

        /*ImageButton play = (ImageButton) this.findViewById(R.id.button_play);
play.setOnClickListener(new OnClickListener() {
public void onClick(View v) {
is_playing_desired = true;
nativePlay();
}
});

ImageButton pause = (ImageButton) this.findViewById(R.id.button_stop);
pause.setOnClickListener(new OnClickListener() {
public void onClick(View v) {
is_playing_desired = false;
nativePause();
}
});*/

        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surface_video);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);

        if (savedInstanceState != null) {
            is_playing_desired = savedInstanceState.getBoolean("playing");
            Log.i ("GStreamer", "Activity created. Saved state is playing:" + is_playing_desired);
        } else {
            is_playing_desired = false;
            Log.i ("GStreamer", "Activity created. There is no saved state, playing: false");
        }

        // Start with disabled buttons, until native code is initialized
        /*this.findViewById(R.id.button_play).setEnabled(false);
this.findViewById(R.id.button_stop).setEnabled(false);*/

        nativeInit();
    }

    protected void onSaveInstanceState (Bundle outState) {
        Log.d ("GStreamer", "Saving state, playing:" + is_playing_desired);
        outState.putBoolean("playing", is_playing_desired);
    }

    protected void onDestroy() {
        nativeFinalize();
        super.onDestroy();
    }

    // Called from native code. This sets the content of the TextView from the UI thread.
    private void setMessage(final String message) {
      /* final TextView tv = (TextView) this.findViewById(R.id.textview_message);
runOnUiThread (new Runnable() {
public void run() {
tv.setText(message);
}
});*/
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void onGStreamerInitialized () {
        Log.i ("GStreamer", "Gst initialized. Restoring state, playing:" + is_playing_desired);
        // Restore previous playing state
        //if (is_playing_desired) {
            nativePlay();
        //} else {
        // nativePause();
       // }

        // Re-enable buttons, now that GStreamer is initialized
        /*final Activity activity = this;
runOnUiThread(new Runnable() {
public void run() {
activity.findViewById(R.id.button_play).setEnabled(true);
activity.findViewById(R.id.button_stop).setEnabled(true);
}
});*/
    }

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("Main");
        nativeClassInit();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) {
        Log.d("GStreamer", "Surface changed to format " + format + " width "
                + width + " height " + height);
        nativeSurfaceInit (holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface created: " + holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface destroyed");
        nativeSurfaceFinalize ();
    }
    
    @Override
public boolean onCreateOptionsMenu(Menu menu) {
// Inflate the menu; this adds items to the action bar if it is present.
getMenuInflater().inflate(R.menu.menu, menu);
return true;
}
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
            case R.id.communicate_rpi:
                Intent intent = new Intent(getBaseContext(), Communicate_Rpi.class);
                startActivity(intent);
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }
    

}