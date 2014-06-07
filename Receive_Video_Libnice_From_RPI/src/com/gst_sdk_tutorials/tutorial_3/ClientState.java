package com.gst_sdk_tutorials.tutorial_3;

import java.util.ArrayList;
import java.util.Locale;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.widget.ListView;

/**
 * Using listview to show clients's state 
 */

public class ClientState extends Activity {

	private ListView mListview;
	
	private ArrayList<ClientInfo> mClientArray = new ArrayList<ClientInfo>();
	
	private native String nativeListOnlineClient (String username);
	
	private String mUserName = null;
	
	private ClientStateAdapter mClientStateAdapter = null;
	
	private ListClientState mListClientStateThread = null;
	
	private native void nativeCloseSocket();
	
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.clients_state_layout);
		
		Log.i("", "onCreate()");
		
		/*
		 * Get data from Login activity 
		 */
		Intent intent = getIntent();
		mUserName = intent.getStringExtra("username");
		
		/*
		 * Initialize listview
		 */
		mClientStateAdapter = new ClientStateAdapter(this, mClientArray, mUserName);
		mListview = (ListView) findViewById(R.id.client_state_listview_id);
		mListview.setAdapter(mClientStateAdapter);
	}

	/**
	 * Initialize ActionBar
	 */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.client_state_actionbar, menu);
		return super.onCreateOptionsMenu(menu);
	}
	
	@Override
	protected void onResume() {
		super.onResume();
		
		Log.i("","Start List client Thread.");
		
		/*
		 * Start list client state thread
		 */
		mListClientStateThread = new ListClientState(this);
		mListClientStateThread.start();
	}
	
	@Override
	protected void onStop() {
		super.onStop();
		
		Log.i("", "Stop list client thread");
		
		/*
		 * Stop list client state thread
		 */
		if (mListClientStateThread != null) {
			mListClientStateThread.terminate();
			
			try {
				mListClientStateThread.join();
			} catch (Exception e) {
			}
		}
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		
		Log.i("", "Close socket!");
		nativeCloseSocket();
	}
	
	/**
	 * Thread to list clients's state
	 */
	class ListClientState extends Thread implements Runnable {

		private boolean mIsRunning = true;
		
		private Activity mActivity;
		
		public ListClientState(Activity activity) {
			this.mActivity = activity;
		}

		@Override
		public void run() {
			
			while(mIsRunning) {
				
				/*
				 * Request server to list all clients's state
				 */
				String result = nativeListOnlineClient(mUserName);
				
				if (result == null)
					continue;
				
				/*
				 * Get clients one by one
				 */
				String[] clients = result.split("\\r?\\n");
				
				/*
				 * Cleare old data
				 */
				if (mClientArray.size() > 0)
					mClientArray.clear();
				
				/*
				 * Get client is rpi
				 */
				for (int i = 0; i < clients.length; i++){
					if (clients[i].toLowerCase(Locale.getDefault()).contains("rpi")) {
						String[] parts = clients[i].split(" ");
						
						if (clients[i].toLowerCase(Locale.getDefault()).contains("online"))
							mClientArray.add(new ClientInfo(parts[0], ClientInfo.ONLINE));
						else
							mClientArray.add(new ClientInfo(parts[0], ClientInfo.OFFLINE));
					}
				}
				
				if (mClientStateAdapter != null) {
					Log.i("","Set adapter data");
					mClientStateAdapter.setData(mActivity, mClientArray);
				}

				/*
				 * Wait 5s
				 */
				try {
					Thread.sleep(1000);
				} catch (Exception e) {
				}
			}
		}
		
		public void terminate() {
			this.mIsRunning = false;
		}

	}
}
