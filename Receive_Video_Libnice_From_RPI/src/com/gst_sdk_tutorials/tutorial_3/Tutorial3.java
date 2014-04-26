package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Point;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;
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
    private native void native_request_servo_rotate(int direction);
    private long native_custom_data; // Native code will use this to keep private data
    private long video_receive_native_custom_data;
    private long audio_send_native_custom_data;
    float prevX, prevY, totalX = 0, totalY = 0, distance;
    int rotateThreadHold, angle;
    private int ROTATE_RIGHT_TO_LEFT = 0;
    private int ROTATE_LEFT_TO_RIGHT = 1;

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

        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surface_video);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);
        
        /* Get phone screen size */
        Display display = getWindowManager().getDefaultDisplay(); 
        int screenWidth = display.getWidth();  // deprecated
        int screenHeight = display.getHeight();  // deprecated
        Log.d ("TAG", "width = " + screenWidth + " height = " + screenHeight);
        
        /* Calculate threadhold 
         * screenWidth/2 = 90 degree
         * Would like rotate 5 dregee a time
         * 
         * => threadHold = screenWidth*5/180
         * 	  1280x720: threadHold = 35.55
         */
        
        rotateThreadHold = (screenWidth)/180;  
        
        
        /* Control camera */
        sv.setOnTouchListener(new OnTouchListener() {
			
			@Override
			public boolean onTouch(View arg0, MotionEvent event) {
				switch (event.getAction()) {
				
				case MotionEvent.ACTION_DOWN:
					/* Save started point*/
					prevX = event.getX();
					prevY = event.getY();
					//Log.d ("TAG", "X = " + prevX + " Y = " + prevY);
					break;
					
				case MotionEvent.ACTION_MOVE:
					float x,y;
					x = event.getX();
					y = event.getY();
					
					float dx = x - prevX;
					float dy = y - prevY;
					
					totalX += dx;
					totalY += dy;
					
					//distance = (float) Math.sqrt( totalX * totalX + totalY * totalY);
					
					/* If distance lager than threadhold -> rotate servo */
					//if (distance > rotateThreadHold)
					if (Math.abs(totalX) > rotateThreadHold)
					{
						Log.d ("TAG", "servo rotate");
						Log.d ("TAG", "totoalX = " + totalX + " totalY = " + totalY);
						Log.d ("TAG", "distance = " + distance);
						
						/* Rotate 5 degree a time */
						if (totalX < 0)
							native_request_servo_rotate (ROTATE_RIGHT_TO_LEFT);
						else
							native_request_servo_rotate (ROTATE_LEFT_TO_RIGHT);
						
						totalX = 0;
						totalY = 0;
					}
					
					prevX = x;
					prevY = y;
					break;
					
				case MotionEvent.ACTION_UP:
					/*Log.i ("TAG", "totoalX = " + totalX + " totalY = " + totalY);
					totalX = 0;
					totalY = 0;*/
					break;
					
				default:
					break;
					
				}
				return true;
			}
		});
        
        if (savedInstanceState != null) {
            is_playing_desired = savedInstanceState.getBoolean("playing");
            Log.i ("GStreamer", "Activity created. Saved state is playing:" + is_playing_desired);
        } else {
            is_playing_desired = false;
            Log.i ("GStreamer", "Activity created. There is no saved state, playing: false");
        }

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
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void onGStreamerInitialized () {
        Log.i ("GStreamer", "Gst initialized. Restoring state, playing:" + is_playing_desired);
        nativePlay();
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