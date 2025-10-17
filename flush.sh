#!/bin/bash

echo "Se limpiaran los siguientes archivos de memoria compartida:"
ls -l /dev/shm/

rm -rf /dev/shm/*
echo "shared memory limpiada"
ls -l /dev/shm/
