/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package android.app.task;

import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;

/**
 * Contains the parameters used to configure/identify your task. You do not create this object
 * yourself, instead it is handed in to your application by the System.
 */
public class TaskParams implements Parcelable {

    private final int taskId;
    private final Bundle extras;
    private final IBinder mCallback;

    /**
     * @return The unique id of this task, specified at creation time.
     */
    public int getTaskId() {
        return taskId;
    }

    /**
     * @return The extras you passed in when constructing this task with
     * {@link android.content.Task.Builder#setExtras(android.os.Bundle)}. This will
     * never be null. If you did not set any extras this will be an empty bundle.
     */
    public Bundle getExtras() {
        return extras;
    }

    /**
     * @hide
     */
    public ITaskCallback getCallback() {
        return ITaskCallback.Stub.asInterface(mCallback);
    }

    private TaskParams(Parcel in) {
        taskId = in.readInt();
        extras = in.readBundle();
        mCallback = in.readStrongBinder();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(taskId);
        dest.writeBundle(extras);
        dest.writeStrongBinder(mCallback);
    }

    public static final Creator<TaskParams> CREATOR = new Creator<TaskParams>() {
        @Override
        public TaskParams createFromParcel(Parcel in) {
            return new TaskParams(in);
        }

        @Override
        public TaskParams[] newArray(int size) {
            return new TaskParams[size];
        }
    };
}
