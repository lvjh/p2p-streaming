package com.gst_sdk_tutorials.tutorial_3;

import java.util.ArrayList;
import java.util.Locale;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.ListView;

public class ClientState extends Activity {

	private ListView mListview;
	
	private ArrayList<ClientInfo> mClientArray = new ArrayList<ClientInfo>();
	
	private native void nativeCloseSocket();
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.clients_state_layout);
		
		/*
		 * Get data from Login activity 
		 */
		Intent intent = getIntent();
		String result = intent.getStringExtra("Clients Information");
		
		/*
		 * Get clients one by one
		 */
		String[] clients = result.split("\\r?\\n");
		
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
		
		ClientStateAdapter adapter = new ClientStateAdapter(this, mClientArray);
		mListview = (ListView) findViewById(R.id.client_state_listview_id);
		mListview.setAdapter(adapter);
		
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		
		/*
		 * Close socket
		 */
		nativeCloseSocket();
	}
	
	
}
