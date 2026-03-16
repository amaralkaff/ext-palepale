FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY main.py .
COPY files/ ./files/
EXPOSE 8080
CMD ["python", "main.py"]
