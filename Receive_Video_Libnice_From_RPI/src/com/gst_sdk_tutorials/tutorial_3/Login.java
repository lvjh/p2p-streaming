package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class Login extends Activity{

	EditText username, pass;
	Button login;
	int ret = -1;
	private String TAG = "P2P_STREAM";
	
	private native int nativeLogin(String username, String pass);
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		setContentView(R.layout.login);
		
		username = (EditText) findViewById(R.id.editUserName);
		pass = (EditText) findViewById(R.id.editPassword);
		login = (Button) findViewById(R.id.buttonLogin);
		
	}
	
	
	public void login(View v)
	{
		//long start = System.nanoTime();    

		Log.i("TAG", "Start Login!");
		ret = nativeLogin(username.getText().toString(), pass.getText().toString());
		//long elapsedTime = System.nanoTime() - start;
		
		//Log.i("TAG", "Elapsed time " + elapsedTime/1000000000);
		
		if (ret == 0)
		{
			//Toast.makeText(this, "Login success!", Toast.LENGTH_LONG).show();
			Log.i("TAG", "Login sucess!");
			Intent intent = new Intent(this, Tutorial3.class);
			startActivity(intent);
		}
		else if (ret == 1)
			Log.i("TAG", "Login failed!");
	}	
	
	static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("Main");
    }

}
