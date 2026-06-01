# Face Attendance API Specification

## Overview

The Face Attendance API is a REST API that bridges the React Native mobile frontend with the Python backend services for face recognition and attendance tracking.

**Base URL:** `http://localhost:5000`

**API Version:** 1.0.0

**Content-Type:** `application/json` (or `multipart/form-data` for file uploads)

## HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | OK - Request successful |
| 201 | Created - Resource created |
| 400 | Bad Request - Invalid parameters |
| 401 | Unauthorized - Missing/invalid auth |
| 404 | Not Found - Endpoint doesn't exist |
| 413 | Payload Too Large - File exceeds limit |
| 500 | Server Error - Internal server error |
| 503 | Service Unavailable - Server down |

## Standard Response Format

### Success Response

```json
{
  "status": "success",
  "message": "Optional message",
  "data": {
    // Response specific data
  },
  "timestamp": "2024-06-01T10:30:00Z"
}
```

### Error Response

```json
{
  "status": "failed",
  "message": "Error description",
  "error_code": "ERROR_CODE",
  "timestamp": "2024-06-01T10:30:00Z"
}
```

## Endpoints

### 1. Health Check

**Endpoint:** `GET /`

**Description:** Check API server status

**Parameters:** None

**Response:**
```json
{
  "status": "success",
  "message": "Face Attendance API Server is running",
  "version": "1.0.0",
  "timestamp": "2024-06-01T10:30:00.000Z"
}
```

**Example:**
```bash
curl http://localhost:5000/
```

---

### 2. Get Configuration

**Endpoint:** `GET /config`

**Description:** Get server configuration and constraints

**Parameters:** None

**Response:**
```json
{
  "status": "success",
  "config": {
    "max_file_size": 5242880,
    "allowed_extensions": ["png", "jpg", "jpeg", "gif", "webp"],
    "batch_size": 50,
    "sync_enabled": true
  }
}
```

**Example:**
```bash
curl http://localhost:5000/config
```

---

### 3. Face Recognition

**Endpoint:** `POST /recognize`

**Description:** Recognize a person's face in an image

**Content-Type:** `multipart/form-data`

**Parameters:**
- `frame` (file, required) - Image file containing face to recognize
  - Max size: 5MB
  - Formats: PNG, JPG, JPEG, GIF, WebP

**Response:**
```json
{
  "status": "success",
  "data": {
    "person_id": "emp001",
    "person_name": "John Doe",
    "confidence": 0.95,
    "liveness_verified": true
  }
}
```

**Error Cases:**
```json
{
  "status": "failed",
  "message": "No face detected in image"
}
```

**Examples:**

```bash
# Using curl
curl -X POST \
  -F "frame=@/path/to/image.jpg" \
  http://localhost:5000/recognize

# Using Python requests
import requests
with open('image.jpg', 'rb') as f:
    files = {'frame': f}
    response = requests.post('http://localhost:5000/recognize', files=files)
    print(response.json())

# Using JavaScript fetch
const formData = new FormData();
formData.append('frame', imageFile);
const response = await fetch('http://localhost:5000/recognize', {
  method: 'POST',
  body: formData
});
const data = await response.json();
```

---

### 4. Liveness Check

**Endpoint:** `POST /liveness-check`

**Description:** Check if a face in the image is live (not a photo/mask)

**Content-Type:** `multipart/form-data`

**Parameters:**
- `frame` (file, required) - Image file to check for liveness
  - Max size: 5MB
  - Formats: PNG, JPG, JPEG, GIF, WebP

**Response:**
```json
{
  "status": "success",
  "verified": true,
  "confidence": 0.92
}
```

**Response Parameters:**
- `verified` (boolean) - Whether face is live or spoofed
- `confidence` (number) - Confidence score [0-1]

**Example:**
```bash
curl -X POST \
  -F "frame=@/path/to/image.jpg" \
  http://localhost:5000/liveness-check
```

---

### 5. Sync Attendance

**Endpoint:** `POST /sync-attendance`

**Description:** Synchronize attendance records from mobile to backend

**Content-Type:** `application/json`

**Request Body:**
```json
{
  "records": [
    {
      "id": 1,
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00Z",
      "confidence": 0.95,
      "synced": false
    },
    {
      "id": 2,
      "person_id": "emp002",
      "timestamp": "2024-06-01T10:31:00Z",
      "confidence": 0.89,
      "synced": false
    }
  ],
  "timestamp": "2024-06-01T10:31:00Z",
  "client_info": {
    "platform": "react-native",
    "version": "1.0.0"
  }
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Synced 2 records",
  "synced_count": 2,
  "total_records": 2
}
```

**Error Cases:**
```json
{
  "status": "failed",
  "message": "No records provided"
}
```

**Example:**

```bash
# Using curl
curl -X POST http://localhost:5000/sync-attendance \
  -H "Content-Type: application/json" \
  -d '{
    "records": [{
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00Z",
      "confidence": 0.95
    }],
    "timestamp": "2024-06-01T10:30:00Z",
    "client_info": {"platform": "react-native", "version": "1.0.0"}
  }'

# Using JavaScript
const response = await fetch('http://localhost:5000/sync-attendance', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    records: attendanceRecords,
    timestamp: new Date().toISOString(),
    client_info: { platform: 'react-native', version: '1.0.0' }
  })
});
const data = await response.json();
```

---

### 6. Get Persons List

**Endpoint:** `GET /persons`

**Description:** Get list of all registered employees/persons

**Parameters:** None

**Response:**
```json
{
  "status": "success",
  "persons": [
    {
      "id": "emp001",
      "name": "John Doe"
    },
    {
      "id": "emp002",
      "name": "Jane Smith"
    },
    {
      "id": "emp003",
      "name": "Mike Johnson"
    }
  ],
  "count": 3
}
```

**Example:**
```bash
curl http://localhost:5000/persons
```

---

### 7. Get Attendance Statistics

**Endpoint:** `GET /attendance-stats`

**Description:** Get database statistics for attendance records

**Parameters:** None

**Response:**
```json
{
  "status": "success",
  "stats": {
    "total": 150,
    "synced": 145,
    "unsynced": 5
  }
}
```

**Example:**
```bash
curl http://localhost:5000/attendance-stats
```

---

### 8. Get Attendance Records

**Endpoint:** `GET /attendance-records`

**Description:** Get unsynced attendance records from server

**Parameters:**
- `limit` (query, optional) - Max records to return (default: 100)

**Response:**
```json
{
  "status": "success",
  "records": [
    {
      "id": 1,
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00Z",
      "confidence": 0.95
    }
  ],
  "count": 1
}
```

**Example:**
```bash
curl http://localhost:5000/attendance-records?limit=50
```

---

## Data Types

### AttendanceRecord

```json
{
  "id": 1,
  "person_id": "emp001",
  "timestamp": "2024-06-01T10:30:00Z",
  "confidence": 0.95,
  "synced": false
}
```

**Fields:**
- `id` (integer) - Unique record ID
- `person_id` (string) - Employee ID
- `timestamp` (string) - ISO 8601 datetime
- `confidence` (number) - Face match confidence [0-1]
- `synced` (boolean) - Sync status

### Person

```json
{
  "id": "emp001",
  "name": "John Doe"
}
```

**Fields:**
- `id` (string) - Unique person ID
- `name` (string) - Person's full name

### FaceRecognitionResult

```json
{
  "person_id": "emp001",
  "person_name": "John Doe",
  "confidence": 0.95,
  "liveness_verified": true
}
```

**Fields:**
- `person_id` (string) - Matched person ID
- `person_name` (string) - Matched person's name
- `confidence` (number) - Match confidence [0-1]
- `liveness_verified` (boolean) - Face is live

### LivenessResult

```json
{
  "verified": true,
  "confidence": 0.92
}
```

**Fields:**
- `verified` (boolean) - Face is live
- `confidence` (number) - Verification confidence [0-1]

---

## Rate Limiting

Default rate limits (can be customized):
- **Global:** 200 requests per day, 50 per hour
- **Per endpoint:** May vary

Response when rate limited:
```json
{
  "status": "failed",
  "message": "Rate limit exceeded"
}
```

HTTP Status: `429 Too Many Requests`

---

## Authentication (Optional)

For production, add Bearer token authentication:

```bash
curl -H "Authorization: Bearer YOUR_TOKEN" http://localhost:5000/persons
```

---

## CORS Headers

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
```

---

## Error Codes

| Code | Message |
|------|---------|
| INVALID_FILE_FORMAT | File format not supported |
| FILE_TOO_LARGE | File size exceeds maximum |
| NO_FACE_DETECTED | No face found in image |
| FACE_NOT_RECOGNIZED | Face doesn't match any person |
| LIVENESS_FAILED | Face appears to be spoofed |
| INVALID_PARAMETERS | Missing or invalid parameters |
| DATABASE_ERROR | Database operation failed |
| SERVER_ERROR | Internal server error |

---

## Request/Response Examples

### Complete Flow Example

**1. Get employees:**
```bash
curl http://localhost:5000/persons
```

**2. Recognize face:**
```bash
curl -X POST -F "frame=@photo.jpg" http://localhost:5000/recognize
```

**3. Check liveness:**
```bash
curl -X POST -F "frame=@photo.jpg" http://localhost:5000/liveness-check
```

**4. Sync attendance:**
```bash
curl -X POST http://localhost:5000/sync-attendance \
  -H "Content-Type: application/json" \
  -d '{
    "records": [{
      "person_id": "emp001",
      "timestamp": "2024-06-01T10:30:00Z",
      "confidence": 0.95
    }],
    "timestamp": "2024-06-01T10:30:00Z",
    "client_info": {"platform": "react-native", "version": "1.0.0"}
  }'
```

---

## Best Practices

1. **Image Quality:**
   - Provide clear, well-lit images
   - Face should be front-facing
   - Minimum 480x640 resolution recommended

2. **Batch Operations:**
   - Use batch sync for multiple records
   - Batch size: 50 records recommended
   - Implement retry logic for failed syncs

3. **Error Handling:**
   - Check `status` field first
   - Implement exponential backoff for retries
   - Log failed requests for debugging

4. **Performance:**
   - Cache persons list locally
   - Use pagination for large datasets
   - Compress images before upload

5. **Security:**
   - Validate file types on client
   - Use HTTPS in production
   - Implement authentication tokens
   - Rate limiting to prevent abuse

---

## Testing with Postman

1. Import collection:
   - Create new Postman collection
   - Add endpoints with examples above
   - Test each endpoint

2. Environment variables:
   ```json
   {
     "base_url": "http://localhost:5000",
     "token": "YOUR_TOKEN"
   }
   ```

3. Test scripts:
   - Add test assertions
   - Validate response structure
   - Check status codes

---

## Webhooks (Future)

Planned webhook support:
```
POST /webhooks/attendance-synced
POST /webhooks/face-recognized
POST /webhooks/sync-failed
```

---

## API Changelog

### v1.0.0 (Current)
- Initial API release
- Face recognition endpoint
- Liveness detection endpoint
- Attendance sync endpoint
- Person database endpoint

### Future Versions
- v1.1.0: Webhook support
- v1.2.0: Advanced analytics
- v2.0.0: GraphQL API

---

## Support

For API support:
- Check error messages
- Review logs
- Test with provided examples
- Contact development team
