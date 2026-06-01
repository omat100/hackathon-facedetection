from storage import AttendanceStorage

class AWSSync:
    def __init__(self):
        self.storage = AttendanceStorage()
        self._init_aws()

    def sync(self):
        if not self.is_connected():
            print("⚠️ No internet — sync skipped")
            return False, 0

        if not self.dynamodb:
            print("⚠️ AWS not configured")
            return False, 0

        pending = self.storage.get_unsynced()

        if not pending:
            print("✅ Nothing to sync")
            return True, 0

        print(f"Syncing {len(pending)} records...")

        table      = self.dynamodb.Table(AWS_CONFIG["dynamodb_table"])
        synced_ids = []

        for record in pending:
            try:
                table.put_item(Item={
                    "person_id":  record["person_id"],
                    "timestamp":  record["timestamp"],
                    "confidence": str(record["confidence"]),
                    "device_id":  os.uname().nodename,
                    "synced_at":  datetime.now().isoformat()
                })
                synced_ids.append(record["id"])
            except Exception as e:
                print(f"❌ Failed: {record['person_id']} — {e}")

        if synced_ids:
            self.storage.mark_synced(synced_ids)
            self.storage.purge_synced()
            print(f"✅ Synced {len(synced_ids)} records — purged from local")

        return True, len(synced_ids)