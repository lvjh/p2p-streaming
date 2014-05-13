package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.content.Intent;
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
import android.widget.TextView;
import android.widget.Toast;

import com.gstreamer.GStreamer;

public class Tutorial3 extends Activity implements SurfaceHolder.Callback {
    
	/* Gstreamer */
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
    
    /* Servo */
    private float prevX, prevY, totalX = 0, totalY = 0, distance;
    private int rotateThreadHoldX, rotateThreadHoldY, angle;
    private int ROTATE_RIGHT_TO_LEFT = 0;
    private int ROTATE_LEFT_TO_RIGHT = 1;
    private int ROTATE_TOP_TO_BOTTOM = 2;
    private int ROTATE_BOTTOM_TO_TOP = 3;
    private native void native_request_servo_rotate(int direction);
    
    /* Temperature sensor */
    private Thread getTemperature;
    private boolean getTemperatureIsRunning = true;
    private native void native_get_temperature();
    
    /* Piezosiren */
    private int PIEZO_OFF = 0x1;
    private int PIEZO_ON  = 0x2;
    private int piezoStatus = PIEZO_OFF;
    private native void native_control_piezo(int status);

    // Called when the activity is first created
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Initialize GStreamer and warn if it fails
        try 
        {
            GStreamer.init(this);
        }
        catch (Exception e) 
        {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.main);

        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surface_video);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);
        sv.setOnTouchListener(SurfaceviewOnTouchListener);
        
        getTemperature = new Thread(getTempRunnable);
        getTemperature.start();
        
        /* Get phone screen size */
        Display display = getWindowManager().getDefaultDisplay(); 
        int screenWidth = display.getWidth();  // deprecated
        int screenHeight = display.getHeight();  // deprecated
        Log.d ("TAG", "width = " + screenWidth + " height = " + screenHeight);
        
        rotateThreadHoldX = (screenWidth)/180;  
        rotateThreadHoldY = (screenHeight)*2/180;
        
        if (savedInstanceState != null) {
            is_playing_desired = savedInstanceState.getBoolean("playing");
            Log.i ("GStreamer", "Activity created. Saved state is playing:" + is_playing_desired);
        } else {
            is_playing_desired = false;
            Log.i ("GStreamer", "Activity created. There is no saved state, playing: false");
        }

        nativeInit();
    }

    /* Control servo */
    OnTouchListener SurfaceviewOnTouchListener = new OnTouchListener() {
		
		@Override
		public boolean onTouch(View v, MotionEvent event) {
			switch (event.getAction()) 
	    	{
				case MotionEvent.ACTION_DOWN:
					/* Save started point*/
					prevX = event.getX();
					prevY = event.getY();
					break;
					
				case MotionEvent.ACTION_MOVE:
					float x,y;
					x = event.getX();
					y = event.getY();
					
					float dx = x - prevX;
					float dy = y - prevY;
					
					totalX += dx;
					totalY += dy;
					
					/* If distance lager than threadhold -> rotate servo */
					//if (distance > rotateThreadHold)
					if (Math.abs(totalX) > rotateThreadHoldX)
					{
						Log.d ("TAG", "totoalX = " + totalX);
						
						/* Rotate 5 degree a time */
						if (totalX < 0)
							native_request_servo_rotate (ROTATE_RIGHT_TO_LEFT);
						else
							native_request_servo_rotate (ROTATE_LEFT_TO_RIGHT);
						
						totalX = 0;
					}
					
					if (Math.abs(totalY) > rotateThreadHoldY)
					{
						Log.d ("TAG", "totalY = " + totalY);
						
						/* Rotate 5 degree a time */
						if (totalY < 0)
							native_request_servo_rotate (ROTATE_BOTTOM_TO_TOP);
						else
							native_request_servo_rotate (ROTATE_TOP_TO_BOTTOM);
						
						totalY = 0;
					}
					
					prevX = x;
					prevY = y;
					
					break;
					
				default:
					break;
			}
	    	
			return true;
		}
	};
    
    /* Thread to send control to get temperature from Rpi */
    Runnable getTempRunnable = new Runnable() 
    {
		@Override
		public void run() {
			while (getTemperatureIsRunning)
			{
				native_get_temperature();
				
				try 
				{
					Thread.sleep(1000);
				} 
				catch (InterruptedException e) 
				{
					e.printStackTrace();
				}
			}
			
			Log.d("TAG", "Stop get temperature!");
		}
	};
	
	/* Display Temperature */
	 private void setMessage(final String message) 
	 {
	        final TextView tv = (TextView) this.findViewById(R.id.textview_message);
	        runOnUiThread (new Runnable() 
	        {
				public void run() 
				{
					tv.setText("Temperature = " + message);
				}
	        });
	 }

	 /* Control piezosiren */
	 public void PiezoOnClick(View view)
	 {
		 if (piezoStatus == PIEZO_OFF)
		 {
		 	native_control_piezo(PIEZO_ON);
		 	piezoStatus = PIEZO_ON;
		 }
		 else if (piezoStatus == PIEZO_ON)
		 {
			 native_control_piezo(PIEZO_OFF);
			 piezoStatus = PIEZO_OFF;
		 }
	 }
	 
	@Override
	protected void onStop() 
	{
   		super.onStop();
   		getTemperatureIsRunning = false;
   	}
    
    protected void onDestroy() 
    {
        nativeFinalize();
        super.onDestroy();
    }

    protected void onSaveInstanceState (Bundle outState) 
    {
        Log.d ("GStreamer", "Saving state, playing: " + is_playing_desired);
        outState.putBoolean("playing", is_playing_desired);
    }
    
    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void onGStreamerInitialized () 
    {
        Log.i ("GStreamer", "Gst initialized. Restoring state, playing:" + is_playing_desired);
        nativePlay();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) 
    {
        Log.d("GStreamer", "Surface changed to format " + format + " width "
                + width + " height " + height);
        nativeSurfaceInit (holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) 
    {
        Log.d("GStreamer", "Surface created: " + holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder)
    {
        Log.d("GStreamer", "Surface destroyed");
        nativeSurfaceFinalize ();
    }
    
    static 
    {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("Main");
        nativeClassInit();
    }

//    @Override
//	public boolean onCreateOptionsMenu(Menu menu) {
//		// Inflate the menu; this adds items to the action bar if it is present.
//		getMenuInflater().inflate(R.menu.menu, menu);
//		return true;
//	}
    
//    @Override
//    public boolean onOptionsItemSelected(MenuItem item) {
//        // Handle item selection
//        switch (item.getItemId()) {
//            case R.id.communicate_rpi:
//                Intent intent = new Intent(getBaseContext(), Communicate_Rpi.class);
//                startActivity(intent);
//                return true;
//            default:
//                return super.onOptionsItemSelected(item);
//        }
//    }

}