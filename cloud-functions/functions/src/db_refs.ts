export const getUserFCMTokenRef = (uid: string) => `users/${uid}/fcm_token`;
export const getUserInfoRef = (uid: string) => `users/${uid}/info`;
export const getSchedulesByDateRef = (date: string) => `schedules/${date}`;
export const getPatientCaretakersRef = (patientUid: string) =>
  `patients/${patientUid}/caretakers`;
