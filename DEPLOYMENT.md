# Deployment Guide

This guide covers deploying the Face Attendance application to production.

## Deployment Architecture

```
┌─────────────────────────────────────────┐
│  Production Environment                 │
├─────────────────────────────────────────┤
│                                         │
│  ┌──────────────────────────────────┐  │
│  │  Mobile App (iOS/Android)        │  │
│  │  - AppStore / Play Store         │  │
│  │  - Offline first architecture    │  │
│  └──────────────┬───────────────────┘  │
│                 │                       │
│                 ▼                       │
│  ┌──────────────────────────────────┐  │
│  │  API Gateway / Load Balancer     │  │
│  │  - Nginx / AWS ALB               │  │
│  │  - SSL/TLS Termination           │  │
│  │  - Rate Limiting                 │  │
│  └──────────────┬───────────────────┘  │
│                 │                       │
│                 ▼                       │
│  ┌──────────────────────────────────┐  │
│  │  API Server (Gunicorn)           │  │
│  │  - Multiple instances            │  │
│  │  - Health checks                 │  │
│  │  - Auto-scaling                  │  │
│  └──────────────┬───────────────────┘  │
│                 │                       │
│                 ▼                       │
│  ┌──────────────────────────────────┐  │
│  │  Services                        │  │
│  │  - Face Recognition              │  │
│  │  - Liveness Detection            │  │
│  │  - Database                      │  │
│  └──────────────────────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```

## Prerequisites

- Cloud hosting (AWS, GCP, Azure, DigitalOcean)
- Docker & Docker Compose
- SSL/TLS certificates
- Domain name
- CI/CD pipeline (GitHub Actions, GitLab CI)
- Monitoring tools (DataDog, New Relic)

## Backend Deployment

### 1. Docker Setup

Create `Dockerfile`:

```dockerfile
FROM python:3.9-slim

WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libsm6 \
    libxext6 \
    libxrender-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy application
COPY . .

# Create uploads directory
RUN mkdir -p uploads

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import requests; requests.get('http://localhost:5000/')"

# Expose port
EXPOSE 5000

# Run application with Gunicorn
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "--workers", "4", "--timeout", "120", "api_server:app"]
```

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  api:
    build: .
    ports:
      - "5000:5000"
    environment:
      - FLASK_ENV=production
      - LOG_LEVEL=INFO
    volumes:
      - ./uploads:/app/uploads
      - ./attendance.db:/app/attendance.db
    restart: always
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:5000/"]
      interval: 30s
      timeout: 10s
      retries: 3
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '1'
          memory: 1G

  nginx:
    image: nginx:latest
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./ssl:/etc/nginx/ssl:ro
    depends_on:
      - api
    restart: always
```

### 2. Gunicorn Configuration

Create `wsgi.py`:

```python
import os
from api_server import app

if __name__ == '__main__':
    app.run(
        host='0.0.0.0',
        port=5000,
        debug=False,
        use_reloader=False
    )
```

Run with Gunicorn:

```bash
pip install gunicorn
gunicorn --bind 0.0.0.0:5000 --workers 4 --worker-class sync --timeout 120 wsgi:app
```

### 3. Nginx Configuration

Create `nginx.conf`:

```nginx
upstream api {
    server api:5000;
}

server {
    listen 80;
    server_name your-domain.com www.your-domain.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name your-domain.com www.your-domain.com;

    # SSL Configuration
    ssl_certificate /etc/nginx/ssl/cert.pem;
    ssl_certificate_key /etc/nginx/ssl/key.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    ssl_prefer_server_ciphers on;

    # Security Headers
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;

    # CORS Headers
    add_header Access-Control-Allow-Origin "*" always;
    add_header Access-Control-Allow-Methods "GET, POST, PUT, DELETE, OPTIONS" always;
    add_header Access-Control-Allow-Headers "Content-Type, Authorization" always;

    # Gzip Compression
    gzip on;
    gzip_types text/plain text/css text/javascript application/json;
    gzip_min_length 1000;

    # Rate Limiting
    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=10r/s;
    limit_req zone=api_limit burst=20 nodelay;

    # File Upload Limit
    client_max_body_size 5M;

    # Proxy Settings
    location / {
        proxy_pass http://api;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_connect_timeout 60s;
        proxy_send_timeout 120s;
        proxy_read_timeout 120s;
    }

    # Health Check
    location /health {
        access_log off;
        proxy_pass http://api;
    }

    # Static Files Cache
    location ~* \.(jpg|jpeg|png|gif|ico|css|js|svg|webp)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
```

### 4. Environment Configuration

Create `.env.production`:

```bash
# Flask
FLASK_ENV=production
FLASK_DEBUG=0

# API Settings
API_HOST=0.0.0.0
API_PORT=5000
API_WORKERS=4

# Database
DATABASE_URL=sqlite:///attendance.db
DATABASE_POOL_SIZE=10

# Logging
LOG_LEVEL=INFO
LOG_FILE=/var/log/api/app.log

# Security
SECRET_KEY=your-secret-key-here
JWT_SECRET=your-jwt-secret-here

# Monitoring
SENTRY_DSN=https://your-sentry-dsn
DATADOG_API_KEY=your-datadog-key
```

### 5. Deploy to AWS

**Using AWS Elastic Beanstalk:**

```bash
# Install EB CLI
pip install awsebcli

# Initialize
eb init -p docker face-attendance-api

# Create environment
eb create production --instance-type t3.medium

# Deploy
eb deploy

# Monitor
eb status
eb logs
```

**Using EC2:**

```bash
# SSH into instance
ssh -i key.pem ubuntu@your-instance-ip

# Install Docker
sudo apt-get update
sudo apt-get install docker.io docker-compose

# Clone repository
git clone your-repo-url
cd hackathon-facedetection

# Build and run
docker-compose up -d

# Check status
docker-compose ps
docker-compose logs api
```

### 6. Kubernetes Deployment

Create `deployment.yaml`:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: face-attendance-api
spec:
  replicas: 3
  selector:
    matchLabels:
      app: face-attendance-api
  template:
    metadata:
      labels:
        app: face-attendance-api
    spec:
      containers:
      - name: api
        image: your-registry/face-attendance-api:latest
        ports:
        - containerPort: 5000
        env:
        - name: FLASK_ENV
          value: "production"
        resources:
          requests:
            cpu: "500m"
            memory: "512Mi"
          limits:
            cpu: "1000m"
            memory: "1Gi"
        livenessProbe:
          httpGet:
            path: /
            port: 5000
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /
            port: 5000
          initialDelaySeconds: 5
          periodSeconds: 5

---
apiVersion: v1
kind: Service
metadata:
  name: face-attendance-api-service
spec:
  type: LoadBalancer
  selector:
    app: face-attendance-api
  ports:
  - protocol: TCP
    port: 80
    targetPort: 5000
```

Deploy:

```bash
kubectl apply -f deployment.yaml
kubectl get service face-attendance-api-service
```

## Frontend Deployment

### 1. Build for Android

```bash
cd frontend/AttendanceApp

# Create EAS account
eas login

# Build
eas build --platform android --release

# Download APK
# Submit to Play Store
```

### 2. Build for iOS

```bash
# Create Apple Developer account and certificates

eas build --platform ios --release

# Download .ipa file
# Submit to App Store
```

### 3. Deploy to Play Store

1. Create Google Play Developer account
2. Prepare app:
   - Update version in app.json
   - Create release build
   - Test on devices

3. Create store listing:
   - App title, description
   - Screenshots
   - Privacy policy

4. Submit for review:
   - Upload APK
   - Fill out questionnaire
   - Wait for review (typically 2-24 hours)

### 4. Deploy to App Store

1. Create Apple Developer account
2. Prepare app:
   - Update version
   - Create certificate
   - Build for submission

3. Create app record in App Store Connect:
   - Bundle ID
   - Privacy policy
   - Screenshots

4. Submit:
   - Upload build with Xcode
   - Fill app information
   - Wait for review (typically 24-48 hours)

## CI/CD Pipeline

### GitHub Actions

Create `.github/workflows/deploy.yml`:

```yaml
name: Deploy

on:
  push:
    branches: [main]

jobs:
  build-and-deploy:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build Docker Image
      run: |
        docker build -t your-registry/face-attendance-api:${{ github.sha }} .
        docker tag your-registry/face-attendance-api:${{ github.sha }} your-registry/face-attendance-api:latest
    
    - name: Push to Registry
      run: |
        echo ${{ secrets.REGISTRY_PASSWORD }} | docker login -u ${{ secrets.REGISTRY_USERNAME }} --password-stdin
        docker push your-registry/face-attendance-api:${{ github.sha }}
        docker push your-registry/face-attendance-api:latest
    
    - name: Deploy to Production
      run: |
        # Deploy scripts
        ssh -i deploy.key user@your-server "cd /app && docker pull your-registry/face-attendance-api:latest && docker-compose up -d"
```

## Monitoring & Logging

### Application Monitoring

```python
# Add to api_server.py
import logging
from pythonjsonlogger import jsonlogger

# JSON Logging
handler = logging.StreamHandler()
formatter = jsonlogger.JsonFormatter()
handler.setFormatter(formatter)
logger.addHandler(handler)

# Structured Logging
logger.info('Face recognized', extra={
    'person_id': person_id,
    'confidence': confidence,
    'timestamp': datetime.now().isoformat()
})
```

### Error Tracking (Sentry)

```python
import sentry_sdk
from sentry_sdk.integrations.flask import FlaskIntegration

sentry_sdk.init(
    dsn=os.environ.get('SENTRY_DSN'),
    integrations=[FlaskIntegration()],
    traces_sample_rate=0.1
)
```

### Metrics (Prometheus)

```python
from prometheus_client import Counter, Histogram, generate_latest

recognition_counter = Counter(
    'face_recognition_total',
    'Total face recognitions',
    ['result']
)

recognition_time = Histogram(
    'face_recognition_duration_seconds',
    'Face recognition duration'
)
```

## Scaling

### Horizontal Scaling

```bash
# Docker Swarm
docker swarm init
docker service create --name api --replicas 3 your-image:latest

# Kubernetes
kubectl scale deployment face-attendance-api --replicas 5
```

### Caching Layer

```python
# Redis caching
from flask_caching import Cache

cache = Cache(app, config={'CACHE_TYPE': 'redis'})

@app.route('/persons')
@cache.cached(timeout=300)
def get_persons():
    # Get persons
    pass
```

### Database Optimization

```sql
-- Create indexes
CREATE INDEX idx_person_id ON attendance(person_id);
CREATE INDEX idx_timestamp ON attendance(timestamp);
CREATE INDEX idx_synced ON attendance(synced);

-- Analyze query performance
EXPLAIN QUERY PLAN SELECT * FROM attendance WHERE person_id = 'emp001';
```

## Backup & Recovery

### Automated Backups

```bash
#!/bin/bash
# backup.sh
BACKUP_DIR="/backups/attendance"
DATE=$(date +%Y%m%d_%H%M%S)

# Database backup
sqlite3 /app/attendance.db ".dump" > "$BACKUP_DIR/attendance_$DATE.sql"

# Upload to S3
aws s3 cp "$BACKUP_DIR/attendance_$DATE.sql" s3://your-bucket/backups/

# Keep only last 30 days
find "$BACKUP_DIR" -mtime +30 -delete
```

### Recovery Procedure

```bash
# Restore from backup
sqlite3 attendance.db < backup_attendance_20240601_120000.sql

# Verify integrity
sqlite3 attendance.db "PRAGMA integrity_check;"
```

## SSL/TLS Configuration

### Generate Self-Signed Certificate (Development)

```bash
openssl req -x509 -newkey rsa:4096 -nodes -out cert.pem -keyout key.pem -days 365
```

### Let's Encrypt (Production)

```bash
# Install Certbot
sudo apt-get install certbot python3-certbot-nginx

# Get certificate
sudo certbot certonly --nginx -d your-domain.com

# Auto-renew
sudo systemctl enable certbot.timer
```

## Performance Tuning

### Database

```python
# Connection pooling
SQLALCHEMY_POOL_SIZE = 20
SQLALCHEMY_POOL_RECYCLE = 3600
SQLALCHEMY_POOL_PRE_PING = True
```

### API

```python
# Compression
from flask_compress import Compress
Compress(app)

# Caching headers
@app.after_request
def add_cache_headers(response):
    response.headers['Cache-Control'] = 'public, max-age=300'
    return response
```

### Images

```python
# Optimize image processing
from PIL import Image
import io

def optimize_image(file):
    img = Image.open(file)
    img.thumbnail((640, 640))
    
    output = io.BytesIO()
    img.save(output, format='JPEG', quality=85)
    output.seek(0)
    return output
```

## Health Checks & Alerts

### Uptime Monitoring

```bash
# Use services like:
# - UptimeRobot
# - StatusCake
# - Datadog Synthetics

curl -f http://your-domain.com/health || send_alert
```

### Alert Thresholds

- Error rate > 1%
- Response time > 500ms
- CPU usage > 80%
- Memory usage > 90%
- Database connection errors

## Disaster Recovery Plan

1. **Daily backups** to S3/GCS
2. **Backup verification** - test restore weekly
3. **Documentation** - keep runbooks updated
4. **Team training** - ensure team knows procedures
5. **Regular drills** - practice recovery quarterly

## Security Checklist

- [ ] SSL/TLS enabled
- [ ] Environment variables secured
- [ ] API authentication implemented
- [ ] Rate limiting configured
- [ ] Input validation enabled
- [ ] SQL injection protection active
- [ ] CORS properly configured
- [ ] Security headers added
- [ ] Secrets rotated regularly
- [ ] Logs monitored for attacks

## Rollback Procedure

```bash
# If deployment fails, rollback
docker service update --image your-registry/image:previous-tag api

# Or with Kubernetes
kubectl rollout undo deployment/face-attendance-api

# Verify
kubectl rollout status deployment/face-attendance-api
```

## Post-Deployment

1. **Test all endpoints** using API spec
2. **Load testing** - use tools like Apache JMeter
3. **Security audit** - penetration testing
4. **User acceptance testing** - with stakeholders
5. **Documentation** - update runbooks
6. **Team training** - ensure team understands deployment

## Support & Escalation

**Tier 1:** Log monitoring, auto-alerts
**Tier 2:** Engineering team review
**Tier 3:** Architecture review, change management

## References

- Docker: https://docs.docker.com
- Kubernetes: https://kubernetes.io/docs
- AWS: https://docs.aws.amazon.com
- Nginx: https://nginx.org/en/docs
- Let's Encrypt: https://letsencrypt.org
