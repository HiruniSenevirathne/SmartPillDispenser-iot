import * as functions from "firebase-functions";
import { IDBSchedulesMap, IScheduleItem } from "./type";
import * as admin from "firebase-admin";
import { initializeApp } from "firebase-admin";
import moment = require("moment");
import { getSchedulesByDateRef } from "./db_refs";
import { scheduleCheckCron, scheduleCheckDiff } from "./config";
import { sendScheduleItemNotifications } from "./push_notifications";

initializeApp();
// // Start writing Firebase Functions
// // https://firebase.google.com/docs/functions/typescript
//
// export const helloWorld = functions.https.onRequest((request, response) => {
//   functions.logger.info("Hello logs!", {structuredData: true});
//   response.send("Hello from Firebase!");
// });

export const updateScheduleQueue = functions.database
  .ref("/patients/{patientUID}/schedule/{scheduleId}")
  .onWrite(async (snapshot, context) => {
    // Grab the current value of what was written to the Realtime Database.

    let isDelete = false;

    const patientUID: string = context.params.patientUID;
    const scheduleId: string = context.params.scheduleId;

    let item: IScheduleItem;

    if (!snapshot.after.exists()) {
      isDelete = true;
      item = snapshot.before.val();
    } else {
      item = snapshot.after.val();
    }

    const newRef = `schedules/${item.date}/${patientUID}/${scheduleId}/`;
    if (isDelete) {
      //Remove item
      const dbRef = admin.database().ref(newRef);
      await dbRef.remove();
    } else {
      const dbRef = admin.database().ref(newRef);
      await dbRef.set(item);
    }

    const patientDaySchedules = `schedules/${item.date}/${patientUID}`;
    const mapOfDaySchedulesRes = await admin
      .database()
      .ref(patientDaySchedules)
      .get();

    let devSchedulestr = "";
    if (mapOfDaySchedulesRes.exists()) {
      const scheduleMapData = mapOfDaySchedulesRes.val();
      Object.keys(scheduleMapData).forEach((scheduleId) => {
        const schedule = scheduleMapData[scheduleId];
        if (
          schedule &&
          schedule.medication_type === "Pills" &&
          schedule.status == "0"
        ) {
          const timeData = schedule.time.split(":");
          devSchedulestr += `${scheduleId}-${timeData[0]}-${timeData[1]},`;
        }
      });
    }
    const deviceScheduleRef = `devices/${item.date}/${patientUID}/`;
    admin.database().ref(deviceScheduleRef).set(devSchedulestr);
  });

/**
 * when device update schedule item status this function adds timestamp to dispensed_time field
 */
export const updateScheduleTakenTime = functions.database
  .ref("/patients/{patientUID}/schedule/{scheduleId}/status")
  .onWrite(async (snapshot, context) => {
    // Grab the current value of what was written to the Realtime Database.

    let isDelete = false;

    const patientUID: string = context.params.patientUID;
    const scheduleId: string = context.params.scheduleId;

    let item: string;

    if (!snapshot.after.exists()) {
      isDelete = true;
      item = snapshot.before.val();
    } else {
      item = snapshot.after.val();
    }
    if (item == "1" && !isDelete) {
      const newRef = `/patients/${patientUID}/schedule/${scheduleId}/dispensed_time`;
      const dbRef = admin.database().ref(newRef);
      // await dbRef.set(moment().utc().add(330, "m").unix());
      await dbRef.set(moment().unix());
    }
  });

export const scheduledFunctionCrontab = functions.pubsub
  .schedule(scheduleCheckCron)
  //  export const scheduledFunctionCrontab = functions.pubsub.schedule('*/5 * * * *')
  .timeZone("America/New_York") // Users can choose timezone - default is America/Los_Angeles
  .onRun(async (context) => {
    // const dbRef = admin.database().ref("testcron");

    const dateStamp = moment().utc().add(330, "m").format("YYYY-MM-DD-HH-mm");
    // await dbRef.set(dateStamp);

    let dtStrList = dateStamp.split("-");
    const dtStr = `${dtStrList[0]}-${dtStrList[1]}-${dtStrList[2]}`;
    const dtData = dateStamp.split("-").map((v) => +v);
    console.info(dtData);

    const cHour = dtData[3];
    const cMinutes = dtData[4];
    const cDayMinutes = cHour * 60 + cMinutes;

    const scheduleDateRefStr = getSchedulesByDateRef(dtStr);
    console.log(scheduleDateRefStr);
    const scheduleDateRef = admin.database().ref(scheduleDateRefStr);
    const schedulesSnapshot = await scheduleDateRef.get();

    const collctedItems: {
      patient_id: string;
      schedule_id: string;
      schedule: IScheduleItem;
    }[] = [];
    if (schedulesSnapshot.exists()) {
      const schedulesMap: IDBSchedulesMap = schedulesSnapshot.val();
      Object.keys(schedulesMap).forEach((patientUID) => {
        const patinetScedules = schedulesMap[patientUID];
        if (patinetScedules) {
          Object.keys(patinetScedules).forEach((scheduleId) => {
            const scheduleItem = patinetScedules[scheduleId];
            if (scheduleItem && scheduleItem.time) {
              const timeInfo = scheduleItem.time.split(":").map((v) => +v);

              if (timeInfo.length == 2) {
                const scheduleDayMinutes = timeInfo[0] * 60 + timeInfo[1];
                if (
                  cDayMinutes <= scheduleDayMinutes &&
                  cDayMinutes + scheduleCheckDiff > scheduleDayMinutes
                ) {
                  collctedItems.push({
                    patient_id: patientUID,
                    schedule_id: scheduleId,
                    schedule: scheduleItem,
                  });
                }
              }
            }
          });
        }
      });
      if (collctedItems.length > 0) {
        await Promise.all(
          collctedItems.map((item) =>
            sendScheduleItemNotifications(
              item.patient_id,
              item.schedule_id,
              item.schedule
            )
          )
        );
      }
    } else {
      // console.log("Schedules not found");
    }
    // const token = await getUserFCMToken("VhsNAlIfZzNlEDn2gJqlaWrPf3A2");
    // sendPushNotification(token, "Hello from cloud function!", "!!!!");
    return null;
  });

/*
ctime = 10;

scheduleTime  =11;

ctime<=scheduleTime && ctime+2 >  scheduleTime 



*/
