#!/bin/bash
# Deploy to GCP Cloud Run
# Usage: ./deploy.sh

PROJECT_ID=$(gcloud config get-value project)
REGION="asia-southeast1"
SERVICE="palepale-server"

echo "Building and deploying to Cloud Run..."
gcloud run deploy $SERVICE \
    --source . \
    --region $REGION \
    --allow-unauthenticated \
    --set-env-vars="API_KEY=palepale_2026_key" \
    --memory=256Mi \
    --cpu=1 \
    --min-instances=0 \
    --max-instances=1 \
    --port=8080

echo "Done! Service URL:"
gcloud run services describe $SERVICE --region $REGION --format='value(status.url)'
