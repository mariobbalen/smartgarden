create table sensor_data (
  id                  bigserial,
  sector              integer,
  soil_moisture       integer      not null check (soil_moisture between 0 and 100),
  luminosity          integer      not null check (luminosity between 0 and 100),
  reservoir_volume_l  numeric(6,2) not null check (reservoir_volume_l >= 0),
  recorded_at         timestamptz  not null,
  created_at          timestamptz  not null default now(),

  primary key (id, sector)
);

alter table sensor_data enable row level security;

create policy "Allow anon insert"
  on sensor_data for insert to anon
  with check (true);

create policy "Allow anon select"
  on sensor_data for select to anon
  using (true);
