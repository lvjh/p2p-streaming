package com.gst_sdk_tutorials.tutorial_3;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.widget.ExpandableListView;

import com.gst_sdk_tutorials.tutorial_3.R;

/**
 * Using listview to show clients's state 
 */

public class ClientState extends Activity {

	private ExpandableListView mListView;
	
	private native String nativeListOnlineClient (String username);
	
	private String mUserName = null;
	
	private ListClientState mListClientStateThread = null;
	
	private native void nativeCloseSocket();
	
	private ArrayList<Client> mParent = new ArrayList<Client>();

	private ClientAdapter mAdapter;
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.state_main_layout);
		
		/*
		 * Get data from Login activity 
		 */
		Intent intent = getIntent();
		mUserName = intent.getStringExtra("username");
		
		mListView = (ExpandableListView) findViewById(R.id.lvExp);
		mAdapter = new ClientAdapter(this, mParent, mUserName);
		mListView.setAdapter(mAdapter);
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
	protected void onPause() {
		super.onPause();
		
		Log.i("", "Pause list client thread");
		
		/*
		 * Stop list client state thread
		 */
		if (mListClientStateThread != null) {
			mListClientStateThread.terminate();
			
			try {
				mListClientStateThread.join();
			} catch (Exception e) {
				e.printStackTrace();
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
				
				/* 
				 * Server Down 
				 * */
				if (result == null)
					continue;
				
				/*
				 * Get clients one by one
				 */
				String[] clients = result.split("\\r?\\n");
				
				/*
				 * Cleare old data
				 */
				if (mParent.size() > 0)
					mParent.clear();
				
				for (int i = 0; i < clients.length; i++){
					if (clients[i].toLowerCase(Locale.getDefault()).contains("rpi")) {
						String[] parts = clients[i].split(" ");
						
						if (clients[i].toLowerCase(Locale.getDefault()).contains("online"))
							mParent.add(new Client(parts[0], Client.ONLINE));
						else
							mParent.add(new Client(parts[0], Client.OFFLINE));
					}
				}
				
				if (mAdapter != null) {
					mAdapter.setData(mParent);
				}

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
