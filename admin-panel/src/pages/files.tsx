import {useCallback, useEffect, useState} from "react";
import styles from "./file.module.css"
import {deleteExerciseMedia, listExercises, updateExerciseMedia} from "../api/client/fitnessAssistantAPI.ts";
import type {Exercise} from "../api/models";

type Status = { kind: "idle" } | { kind: "loading" } | { kind: "ok" | "error", text: string };

interface MediaFile {
  file: File,
  mediaType: "image" | "video"
}

export const FilesPage = () => {
  const [exercises, setExercises] = useState<Exercise[]>();
  const [selectedId, setSelectedId] = useState<string>();
  const [file, setFile] = useState<MediaFile | null>(null);
  const [previewUrl, setPreviewUrl] = useState<string | null>(null);
  const [status, setStatus] = useState<Status>({kind: "idle"});

  const selectedExercise = exercises?.find((e) => e.id === selectedId);

  const loadExercises = useCallback(() =>
    listExercises({type: "basic", limit: 1000, offset: 0})
      .then(setExercises)
      .catch((e: Error) => setStatus({kind: "error", text: `Не удалось загрузить упражнения: ${e.message}`})), []);

  useEffect(() => {
    void loadExercises();
  }, [loadExercises]);

  useEffect(() => {
    if (!file) {
      setPreviewUrl(null);
      return;
    }
    const url = URL.createObjectURL(file.file);
    setPreviewUrl(url);
    return () => URL.revokeObjectURL(url);
  }, [file]);

  const selectExercise = (id?: string) => {
    setSelectedId(id);
    setFile(null);
    setStatus({kind: "idle"});
  };

  const save = async () => {
    if (!selectedExercise?.id || !file) return;
    setStatus({kind: "loading"});
    try {
      const resp = await updateExerciseMedia(selectedExercise.id,
        {media_type: file.mediaType});
      if (!resp.upload_url) throw new Error("сервер не вернул ссылку для загрузки");

      const put = await fetch(resp.upload_url, {
        method: "PUT",
        headers: {"Content-Type": file.file.type || "application/octet-stream"},
        body: file.file,
      });
      if (!put.ok) throw new Error(`хранилище ответило ${put.status}`);

      setFile(null);
      await loadExercises();
      setStatus({kind: "ok", text: "Файл сохранен"});
    } catch (e) {
      setStatus({kind: "error", text: `Не удалось сохранить: ${(e as Error).message}`});
    }
  };

  const remove = async () => {
    if (!selectedExercise?.id || !selectedExercise.media) return;
    if (!window.confirm("Удалить изображение упражнения?")) return;
    setStatus({kind: "loading"});
    try {
      await deleteExerciseMedia(selectedExercise.id);
      await loadExercises();
      setStatus({kind: "ok", text: "Изображение удалено"});
    } catch (e) {
      setStatus({kind: "error", text: `Не удалось удалить: ${(e as Error).message}`});
    }
  };

  const mediaUrl = previewUrl ?? selectedExercise?.media?.url;
  const mediaType = file?.mediaType ?? selectedExercise?.media?.type;
  const busy = status.kind === "loading";

  return (
    <div className={styles.filePage}>
      <div className={styles.exerciseList}>
        {exercises?.map((exercise) =>
          <div
            key={exercise.id}
            className={exercise.id === selectedId ? styles.selected : undefined}
            onClick={() => selectExercise(exercise.id)}
          >
            <label>{exercise.name}</label>
          </div>
        ) ?? <label>Загрузка...</label>}
      </div>
      <div className={styles.picker}>
        <h2>{selectedExercise?.name ?? "Выберите упражнение"}</h2>
        <div className={styles.mediaContainer}>
          {selectedExercise ?
            (mediaUrl === undefined ?
              <label>Файл не задан</label> :
              mediaType === "video" ?
                <video src={mediaUrl} controls /> :
                <img src={mediaUrl}/>
            ):
            <label>Выберите упражнение</label>}
        </div>
        {selectedExercise && (
          <div className={styles.actions}>
            <input
              id="image-input"
              key={selectedId + "image"}
              type="file"
              accept="image/*"
              hidden={true}
              multiple={false}
              onChange={(e) => {
                setFile(!e.target.files?.[0] ? null :{
                  file: e.target.files?.[0] ?? null,
                  mediaType: "image",
                })
              }}
            />
            <label htmlFor="image-input" className={styles.imageInput}>
              Выберите изображение
            </label>
            <input
              id="video-input"
              key={selectedId + "video"}
              type="file"
              accept="video/*"
              hidden={true}
              multiple={false}
              onChange={(e) => {
                setFile(!e.target.files?.[0] ? null :{
                  file: e.target.files?.[0] ?? null,
                  mediaType: "video",
                })
              }}
            />
            <label htmlFor="video-input" className={styles.imageInput}>
              Выберите видео
            </label>
            <button disabled={!file || busy} onClick={save}>Сохранить</button>
            <button disabled={!selectedExercise.media || busy} onClick={remove}>Удалить</button>
          </div>
        )}
        {status.kind === "loading" && <label>Выполняется...</label>}
        {(status.kind === "ok" || status.kind === "error") &&
          <label className={status.kind === "error" ? styles.error : undefined}>{status.text}</label>}
      </div>
    </div>
  )
}
