FROM node:22-slim AS frontend

WORKDIR /frontend

COPY frontend/package.json .
RUN npm install

COPY frontend .
RUN npm run build


FROM python:3.13-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

COPY --from=frontend /frontend/dist ./frontend

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]