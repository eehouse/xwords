/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.widget.ListAdapter;
import android.content.Context;
import android.database.DataSetObserver;

/**
 * Let's see if we can implement a few of these methods just once.
 */
public abstract class XWListAdapter implements ListAdapter {
    private int m_count;

    public XWListAdapter( Context context, int count ) {
        m_count = count;
    }

    public boolean areAllItemsEnabled() { return true; }
    public boolean isEnabled( int position ) { return true; }
    public int getCount() { return m_count; }
    public long getItemId(int position) { return position; }
    public int getItemViewType(int position) { return 0; }
    public int getViewTypeCount() { return 1; }
    public boolean hasStableIds() { return true; }
    public boolean isEmpty() { return getCount() == 0; }
    public void registerDataSetObserver(DataSetObserver observer) {}
    public void unregisterDataSetObserver(DataSetObserver observer) {}
}