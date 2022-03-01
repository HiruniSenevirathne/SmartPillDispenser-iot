import * as admin from "firebase-admin";
import {
  getPatientCaretakersRef,
  getUserFCMTokenRef,
  getUserInfoRef,
} from "./db_refs";
import { IScheduleItem, IUserInfo } from "./type";
export const sendScheduleItemNotifications = async (
  patientId: string,
  scheduleItemId: string,
  scheduleItem: IScheduleItem
) => {
  try {
    const patientInfo = await getUserInfo(patientId);
    if (patientInfo != null) {
      //getting patient fcm token
      const patientFCMToken = await getUserFCMToken(patientId);

      //list caretaker uids by patient id
      const caretakers = await getPatientCaretakesList(patientId);
      const caretakerFcmListRes = await Promise.all(
        caretakers.map((caretakerId) => getUserFCMToken(caretakerId))
      );

      //filter fcm tokens
      const caretakerFcmList = caretakerFcmListRes.filter((v) => v != null);

      let patientMessage = `Comment - ${scheduleItem.comment}`;
      let patientTitle = `Now its time to take your ${scheduleItem.medication_type}, ${scheduleItem.time}`;

      let caretakerMessage = `${scheduleItem.medication_type}, ${scheduleItem.time}, Comment - ${scheduleItem.comment}`;
      let caretakerTitle = `${
        patientInfo.first_name + " " + patientInfo.last_name
      } need to take medicines`;

      const listOfNotificationReq = [];

      listOfNotificationReq.push(
        sendPushNotification(patientFCMToken, patientMessage, patientTitle)
      );
      caretakerFcmList.forEach((caretakerId) => {
        listOfNotificationReq.push(
          sendPushNotification(caretakerId, caretakerMessage, caretakerTitle)
        );
      });
      await Promise.all(listOfNotificationReq);
    }
  } catch (err) {
    console.error(err);
  }
};
export const sendPushNotification = async (
  FCMToken: string,
  message: string,
  title: string
) => {
  const payload = {
    token: FCMToken,
    notification: {
      title: title,
      body: message,
    },
    data: {
      body: message,
    },
  };

  await admin.messaging().send(payload);
};
export const getUserFCMToken = async (userId: string) => {
  try {
    const dbRef = admin.database().ref(getUserFCMTokenRef(userId));
    const token = await dbRef.get();
    return token.exists() ? token.val() : null;
  } catch (err) {
    console.error(err);
  }
  return null;
};

export const getUserInfo = async (
  userId: string
): Promise<IUserInfo | null> => {
  try {
    const dbRef = admin.database().ref(getUserInfoRef(userId));
    const userInfo = await dbRef.get();
    return userInfo.exists() ? userInfo.val() : null;
  } catch (err) {
    console.error(err);
  }
  return null;
};

export const getPatientCaretakesList = async (
  patientId: string
): Promise<string[]> => {
  const caretakers: string[] = [];
  try {
    const dbRef = admin.database().ref(getPatientCaretakersRef(patientId));
    const data = await dbRef.get();
    if (data.exists()) {
      caretakers.push(...Object.keys(data.val()));
    }
  } catch (err) {
    console.error(err);
  }
  return caretakers;
};
