// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.net.CronetEngine;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlRequestException;
import org.chromium.net.UrlResponseInfo;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Activity for managing the Cronet Sample.
 */
public class CronetSampleActivity extends Activity {
    private static final String TAG = "CronetSample";

    private CronetEngine mCronetEngine;

    private String mUrl;
    private TextView mResultText;
    private TextView mReceiveDataText;

    class SimpleUrlRequestCallback extends UrlRequest.Callback {
        private ByteArrayOutputStream mBytesReceived = new ByteArrayOutputStream();
        private WritableByteChannel mReceiveChannel = Channels.newChannel(mBytesReceived);

        @Override
        public void onRedirectReceived(
                UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
            Log.i(TAG, "****** onRedirectReceived ******");
            request.followRedirect();
        }

        @Override
        public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
            Log.i(TAG, "****** Response Started ******");
            Log.i(TAG, "*** Headers Are *** %s", info.getAllHeaders());

            request.read(ByteBuffer.allocateDirect(32 * 1024));
        }

        @Override
        public void onReadCompleted(
                UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
            Log.i(TAG, "****** onReadCompleted ******%s", byteBuffer);

            try {
                mReceiveChannel.write(byteBuffer);
            } catch (IOException e) {
                Log.i(TAG, "IOException during ByteBuffer read. Details: ", e);
            }
            byteBuffer.position(0);
            request.read(byteBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            Log.i(TAG, "****** Request Completed, status code is %d, total received bytes is %d",
                    info.getHttpStatusCode(), info.getReceivedBytesCount());

            final String receivedData = mBytesReceived.toString();
            final String url = info.getUrl();
            final String text = "Completed " + url + " (" + info.getHttpStatusCode() + ")";
            CronetSampleActivity.this.runOnUiThread(new Runnable() {
                public void run() {
                    mResultText.setText(text);
                    mReceiveDataText.setText(receivedData);
                    promptForURL(url);
                }
            });
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, UrlRequestException error) {
            Log.i(TAG, "****** onFailed, error is: %s", error.getMessage());

            final String url = mUrl;
            final String text = "Failed " + mUrl + " (" + error.getMessage() + ")";
            CronetSampleActivity.this.runOnUiThread(new Runnable() {
                public void run() {
                    mResultText.setText(text);
                    promptForURL(url);
                }
            });
        }
    }

    static class SimpleUploadDataProvider extends UploadDataProvider {
        private byte[] mUploadData;
        private int mOffset;

        SimpleUploadDataProvider(byte[] uploadData) {
            mUploadData = uploadData;
            mOffset = 0;
        }

        @Override
        public long getLength() {
            return mUploadData.length;
        }

        @Override
        public void read(final UploadDataSink uploadDataSink, final ByteBuffer byteBuffer)
                throws IOException {
            if (byteBuffer.remaining() >= mUploadData.length - mOffset) {
                byteBuffer.put(mUploadData, mOffset, mUploadData.length - mOffset);
                mOffset = mUploadData.length;
            } else {
                int length = byteBuffer.remaining();
                byteBuffer.put(mUploadData, mOffset, length);
                mOffset += length;
            }
            uploadDataSink.onReadSucceeded(false);
        }

        @Override
        public void rewind(final UploadDataSink uploadDataSink) throws IOException {
            mOffset = 0;
            uploadDataSink.onRewindSucceeded();
        }
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mResultText = (TextView) findViewById(R.id.resultView);
        mReceiveDataText = (TextView) findViewById(R.id.dataView);

        CronetEngine.Builder myBuilder = new CronetEngine.Builder(this);
        myBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_IN_MEMORY, 100 * 1024)
                .enableHTTP2(true)
                .enableQUIC(true);

        mCronetEngine = myBuilder.build();

        String appUrl = (getIntent() != null ? getIntent().getDataString() : null);
        if (appUrl == null) {
            promptForURL("https://");
        } else {
            startWithURL(appUrl);
        }
    }

    private void promptForURL(String url) {
        Log.i(TAG, "No URL provided via intent, prompting user...");
        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Enter a URL");
        LayoutInflater inflater = getLayoutInflater();
        View alertView = inflater.inflate(R.layout.dialog_url, null);
        final EditText urlInput = (EditText) alertView.findViewById(R.id.urlText);
        urlInput.setText(url);
        final EditText postInput = (EditText) alertView.findViewById(R.id.postText);
        alert.setView(alertView);

        alert.setPositiveButton("Load", new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int button) {
                String url = urlInput.getText().toString();
                String postData = postInput.getText().toString();
                startWithURL(url, postData);
            }
        });
        alert.show();
    }

    private void applyPostDataToUrlRequestBuilder(
            UrlRequest.Builder builder, Executor executor, String postData) {
        if (postData != null && postData.length() > 0) {
            UploadDataProvider uploadDataProvider =
                    new SimpleUploadDataProvider(postData.getBytes());
            builder.setHttpMethod("POST");
            builder.addHeader("Content-Type", "application/x-www-form-urlencoded");
            builder.setUploadDataProvider(uploadDataProvider, executor);
        }
    }

    private void startWithURL(String url) {
        startWithURL(url, null);
    }

    private void startWithURL(String url, String postData) {
        Log.i(TAG, "Cronet started: %s", url);
        mUrl = url;

        Executor executor = Executors.newSingleThreadExecutor();
        UrlRequest.Callback callback = new SimpleUrlRequestCallback();
        UrlRequest.Builder builder = new UrlRequest.Builder(url, callback, executor, mCronetEngine);
        applyPostDataToUrlRequestBuilder(builder, executor, postData);
        builder.build().start();
    }

    private void startNetLog() {
        mCronetEngine.startNetLogToFile(
                Environment.getExternalStorageDirectory().getPath() + "/cronet_sample_netlog.json",
                false);
    }

    private void stopNetLog() {
        mCronetEngine.stopNetLog();
    }
}
