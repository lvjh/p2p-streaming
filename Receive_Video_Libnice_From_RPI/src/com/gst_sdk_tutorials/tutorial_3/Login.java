package com.gst_sdk_tutorials.tutorial_3;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

/**
 * Login to server
 * If login is successful then list rpis is online,
 * and save user&password
 */
public class Login extends Activity 
{
	private EditText mUsername, mPassword;
	
	private Button mLoginButton;
	
	private final int LOGIN_SUCCESSED = 0x00;
	
	private final int LOGIN_FAILED = 0x01;
	
	private native int nativeLogin (String mUsername, String mPassword);
	
	private native String nativeListOnlineClient (String username);
	
	
	@Override
	protected void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.login_layout);
		
		mUsername = (EditText) findViewById(R.id.editUserName);
		mPassword = (EditText) findViewById(R.id.editPassword);
		mLoginButton = (Button) findViewById(R.id.buttonLogin);
		
		/*
		 * Get old correct username, password
		 */
		SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
		mUsername.setText(sharedPref.getString("USERNAME", null));
		mPassword.setText(sharedPref.getString("PASSWORD", null));
	}
	
	
	public void login(View v)
	{
		int ret = nativeLogin(mUsername.getText().toString(), mPassword.getText().toString());
		
		switch (ret) {
		
		case LOGIN_SUCCESSED:
			
			//Log.i("TAG", "Login sucess!");
			
			String result = nativeListOnlineClient(mUsername.getText().toString());
			
			/* 
			 * Save username & password 
			 */
			SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
			SharedPreferences.Editor editor = sharedPref.edit();
			editor.putString("USERNAME", mUsername.getText().toString());
			editor.putString("PASSWORD", mPassword.getText().toString());
			editor.commit();
			
			/*
			 * Call ClientState Activity enclose data
			 */
			
			Intent intent = new Intent(this, ClientState.class);
			intent.putExtra("Clients Information", result);
			startActivity(intent);
			
			break;
			
		case LOGIN_FAILED:
			
			/*
			 * Couldn't connect to server because wrong input or 
			 * server failed.
			 */
			DialogFragment dialog = new LoginFailedDialogFragment();
			dialog.show(getFragmentManager(), null);
			
			break;
			
		default:
			break;
		}
	}	
	
	/**
	 * Dialog show when login failed
	 */
	public static class LoginFailedDialogFragment extends DialogFragment {
		
	    @Override
	    public Dialog onCreateDialog(Bundle savedInstanceState) 
	    {
	        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
	        builder.setMessage("Username or password is incorrect.");
	        builder.setPositiveButton("Ok", new OnClickListener() {
				
				@Override
				public void onClick(DialogInterface dialog, int which) {
					dismiss();
				}
			});
	               
	        return builder.create();
	    }
	}
	
	static 
	{
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("Main");
    }

}
