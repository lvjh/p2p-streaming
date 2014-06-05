package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class Login extends Activity 
{
	private EditText mUsername, mPassword;
	private Button mLoginButton;
	private native int nativeLogin (String mUsername, String mPassword);
	
	@Override
	protected void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.login_layout);
		
		mUsername = (EditText) findViewById(R.id.editUserName);
		mPassword = (EditText) findViewById(R.id.editPassword);
		mLoginButton = (Button) findViewById(R.id.buttonLogin);
		
		SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
		mUsername.setText(sharedPref.getString("USERNAME", null));
		mPassword.setText(sharedPref.getString("PASSWORD", null));
	}
	
	
	public void login(View v)
	{
		int ret = nativeLogin(mUsername.getText().toString(), mPassword.getText().toString());
		
		if (ret == 0)
		{
			Log.i("TAG", "Login sucess!");
			Intent intent = new Intent(this, Tutorial3.class);
			startActivity(intent);
			
			/*
			 * Save username & password
			 */
			SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
			SharedPreferences.Editor editor = sharedPref.edit();
			editor.putString("USERNAME", mUsername.getText().toString());
			editor.putString("PASSWORD", mPassword.getText().toString());
			editor.commit();
		}
		else if (ret == 1)
		{
			/*
			 * Couldn't connect to server because wrong input or 
			 * server failed.
			 */
			Toast.makeText(this, "Username or Password is not correct", 
					Toast.LENGTH_SHORT).show();
		}
	}	
	
	static 
	{
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("Main");
    }

}
