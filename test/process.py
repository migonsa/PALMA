import pandas as pd

pd.set_option('display.max_columns', None)
pd.set_option('display.max_rows', None)

df = pd.read_csv('results.csv')
df['prev_time'] = df['TIME'].mask(df['CMD'] != 'STARTING')
dfh = df.groupby('HOST').ffill()
df['elapsed'] = (dfh['TIME']-dfh['prev_time'])
df.drop(columns='prev_time')
df1 = df[df['CMD'] == 'AUTO_ASSIGNED']
df1 = df1.append(df[df['CMD'] == 'SERVER_ASSIGNED'])
ax = df1.hist(column='elapsed',by='CMD')
