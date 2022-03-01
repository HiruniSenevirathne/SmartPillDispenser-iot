export interface IScheduleItem {
  comment: string;
  date: string;
  medication_type: string;
  time: string;
}
export interface IDBSchedulesMap {
  [uid: string]: {
    [scheduleId: string]: IScheduleItem;
  };
}
export interface IUserInfo {
  email: string;
  first_name: string;
  last_name: string;
  mode: string;
  user_id: string;
}
