/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License.
 */

package com.android.systemui.statusbar.phone;

import android.view.View;
import android.support.test.filters.SmallTest;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;

import com.android.systemui.statusbar.AlertingNotificationManager;
import com.android.systemui.statusbar.AlertingNotificationManagerTest;
import com.android.systemui.statusbar.notification.NotificationData;
import com.android.systemui.statusbar.notification.VisualStabilityManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.assertFalse;

import static org.mockito.Mockito.when;

@SmallTest
@RunWith(AndroidTestingRunner.class)
@TestableLooper.RunWithLooper
public class HeadsUpManagerPhoneTest extends AlertingNotificationManagerTest {
    @Rule public MockitoRule rule = MockitoJUnit.rule();

    private HeadsUpManagerPhone mHeadsUpManager;

    @Mock private NotificationGroupManager mGroupManager;
    @Mock private View mStatusBarWindowView;
    @Mock private VisualStabilityManager mVSManager;
    @Mock private StatusBar mBar;

    protected AlertingNotificationManager createAlertingNotificationManager() {
        return mHeadsUpManager;
    }

    @Before
    public void setUp() {
        when(mVSManager.isReorderingAllowed()).thenReturn(true);
        mHeadsUpManager = new HeadsUpManagerPhone(mContext, mStatusBarWindowView, mGroupManager,
                mBar, mVSManager);
        super.setUp();
        mHeadsUpManager.mHandler = mTestHandler;
    }

    @Test
    public void testSnooze() {
        mHeadsUpManager.showNotification(mEntry);

        mHeadsUpManager.snooze();

        assertTrue(mHeadsUpManager.isSnoozed(mEntry.notification.getPackageName()));
    }

    @Test
    public void testSwipedOutNotification() {
        mHeadsUpManager.showNotification(mEntry);
        mHeadsUpManager.addSwipedOutNotification(mEntry.key);

        // Remove should succeed because the notification is swiped out
        mHeadsUpManager.removeNotification(mEntry.key, false /* releaseImmediately */);

        assertFalse(mHeadsUpManager.contains(mEntry.key));
    }

    @Test
    public void testShouldExtendLifetime_swipedOut() {
        mHeadsUpManager.showNotification(mEntry);
        mHeadsUpManager.addSwipedOutNotification(mEntry.key);

        // Notification is swiped so its lifetime should not be extended even if it hasn't been
        // shown long enough
        assertFalse(mHeadsUpManager.shouldExtendLifetime(mEntry));
    }

    @Test
    public void testShouldExtendLifetime_notTopEntry() {
        NotificationData.Entry laterEntry = new NotificationData.Entry(createNewNotification(1));
        laterEntry.row = mRow;
        mHeadsUpManager.showNotification(mEntry);
        mHeadsUpManager.showNotification(laterEntry);

        // Notification is "behind" a higher priority notification so we have no reason to keep
        // its lifetime extended
        assertFalse(mHeadsUpManager.shouldExtendLifetime(mEntry));
    }
}
