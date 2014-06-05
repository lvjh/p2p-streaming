package com.gst_sdk_tutorials.tutorial_3;

import java.util.ArrayList;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

public class ClientStateAdapter extends BaseAdapter{

	private ArrayList<ClientInfo> mClientArralist;
	
	private Context mContext;
	
	public ClientStateAdapter(Context contex, ArrayList<ClientInfo> clientArraylist) {
		this.mContext = contex;
		this.mClientArralist = clientArraylist;
	}

	@Override
	public int getCount() {
		return mClientArralist.size();
	}

	@Override
	public Object getItem(int position) {
		return mClientArralist.get(position);
	}

	@Override
	public long getItemId(int position) {
		return position;
	}

	class ViewHolder
	{
		ImageView mStateIcon;
		TextView mClientName;
		Button mConnectTo;
		
		public ViewHolder(View view)
		{
			mStateIcon  = (ImageView) view.findViewById(R.id.imgView_client_status);
			mClientName = (TextView) view.findViewById(R.id.tv_client_name);
			mConnectTo  = (Button) view.findViewById(R.id.button_connect_to);
			
			/*
			 * Iplement client listener 
			 */
		}
	}
	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		
		ViewHolder holder;
		
		if (convertView == null) 
        {  
             LayoutInflater li = (LayoutInflater) mContext  
                       .getSystemService(Context.LAYOUT_INFLATER_SERVICE);  
             convertView = li.inflate(R.layout.listview_item, null);  

             holder = new ViewHolder(convertView);  
             convertView.setTag(holder);  
        }
        else 
        {  
             holder = (ViewHolder) convertView.getTag();  
        }  
		
		/*
		 * Set state icon
		 */
		if (mClientArralist.get(position).getmClientState() == ClientInfo.ONLINE)
			holder.mStateIcon.setImageResource(R.drawable.online);
		else
			holder.mStateIcon.setImageResource(R.drawable.offline);
		
		/*
		 * Set client's name
		 */
		holder.mClientName.setText(mClientArralist.get(position).getmClientName());
		
		return convertView;
	}


}
