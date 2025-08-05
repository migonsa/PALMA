# -*- coding: utf-8 -*-
"""
Created on Mon Jul 27 08:08:58 2020

@author: telerin
"""

import pandas as pd

pd.set_option('display.max_columns', None)
pd.set_option('display.max_rows', None)

df = pd.read_csv('results5.csv')
df['prev_time'] = df['TIME']
not_auto_assigned = df['CMD'] != 'AUTO_ASSIGNED'
df['prev_time'] = df['prev_time'].mask(not_auto_assigned)
df['ADDR'] = df.groupby('HOST')['ADDR'].ffill(limit=1)
df['COUNT'] = df.groupby('HOST')['COUNT'].ffill(limit=1)
df['prev_time'] = df.groupby('HOST')['prev_time'].ffill(limit=1)
df['elapsed'] = df['TIME'] - df['prev_time']
df = df.dropna(subset=['ADDR'])
df['COUNT'] = df['COUNT'].apply(int, base=0)
df.loc[not_auto_assigned,'COUNT'] = -df['COUNT']
df['in_use'] = df['COUNT'].cumsum()
df.plot(x='TIME', y='in_use', style='-')
print(df['in_use'].describe())
df1 = df.drop(df[df['CMD'] == 'AUTO_ASSIGNED'].index)
ax = df1.hist(column='elapsed',by='CMD',bins=1000)